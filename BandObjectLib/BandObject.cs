//    This file is part of QTTabBar, a shell extension for Microsoft
//    Windows Explorer.
//    Copyright (C) 2002-2022  Pavel Zolnikov, Quizo, Paul Accisano, indiff
//
//    QTTabBar is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    QTTabBar is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with QTTabBar.  If not, see <http://www.gnu.org/licenses/>.

using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using SHDocVw;


namespace BandObjectLib {
    public class BandObject : 
        UserControl, 
        IDeskBand, 
        IDockingWindow, 
        IInputObject, 
        IObjectWithSite, 
        IOleWindow,
        IPersistStream,
        IDpiAwareObject
    {
        private Size _minSize = new Size(-1, -1);
        protected IInputObjectSite BandObjectSite;
        protected WebBrowserClass Explorer;
        protected bool fClosedDW;
        protected bool fFinalRelease;
        protected IntPtr ReBarHandle;
        private RebarBreakFixer RebarSubclass;
        private IAsyncResult result;

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
        
        protected const int S_OK = 0;
        protected const int S_FALSE = 1;
        protected const int E_NOTIMPL = -2147467263;	// _HRESULT_TYPEDEF_(0x80004001L)
        protected const int E_FAIL = -2147467259;    // _HRESULT_TYPEDEF_(0x80004005L)

        // �ж��Ƿ�������־��������Ϊfalse�� ��������. Ĭ���ǹرյģ��ڳ���ѡ�����������������
        // public static bool ENABLE_LOGGER = true;

        // We must subclass the rebar in order to fix a certain bug in 
        // Windows 7.
        internal sealed class RebarBreakFixer : NativeWindow {
            private readonly BandObject parent;
            public bool MonitorSetInfo { get; set; }
            public bool Enabled { get; set; }

            public RebarBreakFixer(IntPtr hwnd, BandObject parent) {
                this.parent = parent;
                Enabled = true;
                MonitorSetInfo = true;
                AssignHandle(hwnd);
            }

            protected override void WndProc(ref Message m) {
                // bandLog("WndProc");
                if(!Enabled) {
                    base.WndProc(ref m);
                    return;
                }

                // When the bars are first loaded, they will always have 
                // RBBS_BREAK set.  Catch RB_SETBANDINFO to fix this.
                if(m.Msg == RB.SETBANDINFO) {
                    if(MonitorSetInfo) {
                        Util2.bandLog("msg SETBANDINFO");
                        REBARBANDINFO pInfo = (REBARBANDINFO)Marshal.PtrToStructure(m.LParam, typeof(REBARBANDINFO));
                        if(pInfo.hwndChild == parent.Handle && (pInfo.fMask & RBBIM.STYLE) != 0) {
                            // Ask the bar if we actually want a break.
                            if(parent.ShouldHaveBreak()) {
                                pInfo.fStyle |= RBBS.BREAK;
                            }
                            else {
                                pInfo.fStyle &= ~RBBS.BREAK;
                            }
                            Marshal.StructureToPtr(pInfo, m.LParam, false);
                        }
                    }
                }
                // Whenever a band is deleted, the RBBS_BREAKs come back!
                // Catch RB_DELETEBAND to fix it.
                else if(m.Msg == RB.DELETEBAND) {
                    Util2.bandLog("msg DELETEBAND");
                    int del = (int)m.WParam;
                    
                    // Look for our band
                    int n = parent.ActiveRebarCount();
                    for(int i = 0; i < n; ++i) {
                        REBARBANDINFO info = parent.GetRebarBand(i, RBBIM.STYLE | RBBIM.CHILD);
                        if(info.hwndChild == parent.Handle) {
                            // Call the WndProc to let the deletion fall 
                            // through, with the break status safely saved
                            // in the info variable.
                            base.WndProc(ref m);

                            // If *we're* the one being deleted, no need to do
                            // anything else.
                            if(i == del) {
                                return;
                            }
                                
                            // Otherwise, our style has been messed with.
                            // Set it back to what it was.
                            info.cbSize = Marshal.SizeOf(info);
                            info.fMask = RBBIM.STYLE;
                            IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf(info));
                            Marshal.StructureToPtr(info, ptr, false);
                            bool reset = MonitorSetInfo;
                            MonitorSetInfo = false;
                            SendMessage(parent.ReBarHandle, RB.SETBANDINFO, (IntPtr)i, ptr);
                            MonitorSetInfo = reset;
                            Marshal.FreeHGlobal(ptr);

                            // Return without calling WndProc twice!
                            return;
                        }
                    }
                }
                base.WndProc(ref m);
            }
        }

        private int ActiveRebarCount() {
            return (int)SendMessage(ReBarHandle, RB.GETBANDCOUNT, IntPtr.Zero, IntPtr.Zero);
        }

        // Determines if the DeskBand is preceded by a break.
        protected bool BandHasBreak() {
            int n = ActiveRebarCount();
            for(int i = 0; i < n; ++i) {
                REBARBANDINFO info = GetRebarBand(i, RBBIM.STYLE | RBBIM.CHILD);
                if(info.hwndChild == Handle) {
                    return (info.fStyle & RBBS.BREAK) != 0;
                }
            }
            return true;
        }

        public virtual void CloseDW(uint dwReserved) {
            Util2.bandLog("CloseDW");
            fClosedDW = true;
            ShowDW(false);
            Dispose(true);
            if(Explorer != null) {
                // Util2.bandLog("ReleaseComObject Explorer");
                Marshal.ReleaseComObject(Explorer);
                Explorer = null;
            }
            if(BandObjectSite != null) {
                Marshal.ReleaseComObject(BandObjectSite);
                BandObjectSite = null;
            }
            if(RebarSubclass != null) {
                RebarSubclass.Enabled = false;
                RebarSubclass = null;
            }
        }

        public virtual void ContextSensitiveHelp(bool fEnterMode) {
        }

        public virtual void GetBandInfo(uint dwBandID, uint dwViewMode, ref DESKBANDINFO pdbi) {
            if((pdbi.dwMask & DBIM.ACTUAL) != 0) {
                pdbi.ptActual.X = Size.Width;
                pdbi.ptActual.Y = Size.Height;
            }
            if((pdbi.dwMask & DBIM.INTEGRAL) != 0) {
                pdbi.ptIntegral.X = -1;
                pdbi.ptIntegral.Y = -1;
            }
            if((pdbi.dwMask & DBIM.MAXSIZE) != 0) {
                pdbi.ptMaxSize.X = pdbi.ptMaxSize.Y = -1;
            }
            if((pdbi.dwMask & DBIM.MINSIZE) != 0) {
                pdbi.ptMinSize.X = MinSize.Width;
                pdbi.ptMinSize.Y = MinSize.Height;
            }
            if((pdbi.dwMask & DBIM.MODEFLAGS) != 0) {
                pdbi.dwModeFlags = DBIMF.NORMAL;
            }
            if((pdbi.dwMask & DBIM.BKCOLOR) != 0) {
                pdbi.dwMask &= ~DBIM.BKCOLOR;
            }
            if((pdbi.dwMask & DBIM.TITLE) != 0) {
                pdbi.wszTitle = null;
            }
        }

        private REBARBANDINFO GetRebarBand(int idx, int fMask) {
            Util2.bandLog("GetRebarBand");
            REBARBANDINFO info = new REBARBANDINFO();
            info.cbSize = Marshal.SizeOf(info);
            info.fMask = fMask;
            IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf(info));
            Marshal.StructureToPtr(info, ptr, false);
            SendMessage(ReBarHandle, RB.GETBANDINFO, (IntPtr)idx, ptr);
            info = (REBARBANDINFO)Marshal.PtrToStructure(ptr, typeof(REBARBANDINFO));
            Marshal.FreeHGlobal(ptr);
            return info;
        }

        public virtual void GetSite(ref Guid riid, out object ppvSite) {
            ppvSite = BandObjectSite;
        }

        public virtual void GetWindow(out IntPtr phwnd) {
            phwnd = Handle;
        }

        public virtual int HasFocusIO() {
            if(!ContainsFocus) {
                return 1;
            }
            return 0;
        }

        protected virtual void OnExplorerAttached() {
        }

        protected override void OnGotFocus(EventArgs e) {
            base.OnGotFocus(e);
            if((!fClosedDW && (BandObjectSite != null)) && IsHandleCreated) {
                Util2.bandLog("OnGotFocus");
                BandObjectSite.OnFocusChangeIS(this, 1);
            }
        }

        protected override void OnLostFocus(EventArgs e) {
            base.OnLostFocus(e);
            if((!fClosedDW && (BandObjectSite != null)) && (ActiveControl == null)) {
                Util2.bandLog("OnLostFocus");
                BandObjectSite.OnFocusChangeIS(this, 0);
            }
        }

        public virtual void ResizeBorderDW(IntPtr prcBorder, object punkToolbarSite, bool fReserved) {
        }

        // Override this to set whether the DeskBand has a break when it is 
        // first displayed
        protected virtual bool ShouldHaveBreak() {
            return true;
        }

        public virtual void SetSite(object pUnkSite) {
            if(Process.GetCurrentProcess().ProcessName == "iexplore") {
                Marshal.ThrowExceptionForHR(E_FAIL);
            }
            if(BandObjectSite != null) {
                Marshal.ReleaseComObject(BandObjectSite);
            }
            if(Explorer != null) {
                Marshal.ReleaseComObject(Explorer);
                Explorer = null;
            }
            BandObjectSite = pUnkSite as IInputObjectSite;
            if(BandObjectSite != null) {
                try {
                    object obj2;
                    ((_IServiceProvider)BandObjectSite).QueryService(ExplorerGUIDs.IID_IWebBrowserApp, ExplorerGUIDs.IID_IUnknown, out obj2);
                    Explorer = (WebBrowserClass)Marshal.CreateWrapperOfType(obj2 as IWebBrowser, typeof(WebBrowserClass));
                    OnExplorerAttached();
                }
                catch  (COMException exception) { // exception
                    Util2.MakeErrorLog(exception, "QueryService CreateWrapperOfType");
                }
            }
            try {
                IOleWindow window = pUnkSite as IOleWindow;
                if(window != null) {
                    window.GetWindow(out ReBarHandle);
                }
            }
            catch (Exception e) // exc
            {
                Util2.MakeErrorLog(e, "BandObject SetSite");
               //  logger.Log(exc);
            }
        }

        public virtual void ShowDW(bool fShow) {
            if(ReBarHandle != IntPtr.Zero && Environment.OSVersion.Version.Major > 5) {
                if(RebarSubclass == null) {
                    RebarSubclass = new RebarBreakFixer(ReBarHandle, this);
                }

                RebarSubclass.MonitorSetInfo = true;
                if(result == null || result.IsCompleted) {    
                    result = BeginInvoke(new UnsetInfoDelegate(UnsetInfo));
                }
            }
            Visible = fShow;
        }

        public virtual int TranslateAcceleratorIO(ref MSG msg) {
            if(((msg.message == 0x100) && ((msg.wParam == ((IntPtr)9L)) || (msg.wParam == ((IntPtr)0x75L)))) && SelectNextControl(ActiveControl, (ModifierKeys & Keys.Shift) != Keys.Shift, true, false, false)) {
                return 0;
            }
            return 1;
        }

        public virtual void UIActivateIO(int fActivate, ref MSG msg) {
            if(fActivate != 0) {
                Control nextControl = GetNextControl(this, true);
                if(nextControl != null) {
                    nextControl.Select();
                }
                Focus();
            }
        }

        private delegate void UnsetInfoDelegate();

        private void UnsetInfo() {
            if(RebarSubclass != null) {
                RebarSubclass.MonitorSetInfo = false;
            }
        }

        public Size MinSize {
            get {
                return _minSize;
            }
            set {
                _minSize = value;
            }
        }

        public virtual void GetClassID(out Guid pClassID) {
            pClassID = Guid.Empty;
        }

        public virtual int IsDirty() {
            return 0;
        }

        public virtual void IPersistStreamLoad(object pStm) {
        }

        public virtual void Save(IntPtr pStm, bool fClearDirty) {
        }

        public virtual int GetSizeMax(out ulong pcbSize) {
            const int E_NOTIMPL = -2147467263;
            pcbSize = 0;
            return E_NOTIMPL;
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();
            // 
            // BandObject
            // 
            this.ForeColor = System.Drawing.Color.Black;
            this.Name = "BandObject";
            this.ResumeLayout(false);
        }

        public int Dpi { get; private set; } 

        public float Scaling
        {
            get
            {
               return (float) this.Dpi / 96f;
            }
        } 

        public void NotifyDpiChanged(int oldDpi, int dpiNew)
        {
            Util2.bandLog("BandObject NotifyDpiChanged oldDpi " + oldDpi + " dpiNew " + dpiNew);
            this.Dpi = dpiNew;
            Action<Control> act = (Action<Control>) null;
            act = (Action<Control>)(
                control =>
                {
                    for (var i = 0; i < control.Controls.Count; i++)
                    {
                        var cc = (Control) control.Controls[i];

                        if (cc is IDpiAwareObject)
                            ((IDpiAwareObject)cc).NotifyDpiChanged(oldDpi, dpiNew);
                        act(cc);
                    }
                }
                /*.ForEach<Control>((Action<Control>) 
                    (c =>
                    {
                        if (c is IDpiAwareObject )
                            ((IDpiAwareObject)c).NotifyDpiChanged(oldDpi, dpiNew);
                        act(c);
                    })*/
            );
            act((Control) this);
            this.OnDpiChanged(oldDpi, dpiNew);
        }


        protected virtual void OnDpiChanged(int oldDpi, int newDpi)
        {
        }
    }

    internal class Util2
    {
        private const bool ENABLE_LOGGER = true;


        public static void bandLog(string optional)
        {
            if (ENABLE_LOGGER)
                bandLog("bandLog", optional);
        }

        

        public static void err(string optional)
        {
            if (ENABLE_LOGGER)
                bandLog("err", optional);

        }

        private static void writeStr(string path, StringBuilder formatLogLine)
        {
            using (StreamWriter writer = new StreamWriter(path, true))
            {
                writer.WriteLine(formatLogLine);
            }
        }

        public static void bandLog(string level, string optional)
        {
            string appdata = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            string appdataQT = Path.Combine(appdata, "QTTabBar");
            if (!Directory.Exists(appdataQT))
            {
                Directory.CreateDirectory(appdataQT);
            }

            Process process = Process.GetCurrentProcess();
            var cThreadId = Thread.CurrentThread.ManagedThreadId;
            var currentThreadId = AppDomain.GetCurrentThreadId();

            string path = Path.Combine(appdataQT, "bandLog.log");
            var formatLogLine = new StringBuilder();
            formatLogLine
                .Append("[")
                .Append(level)
                .Append("]");
            if (process != null)
            {
                formatLogLine
                    .Append(" PID:")
                    .Append(process.Id);
            }
            if (cThreadId != null)
            {
                formatLogLine
                    .Append(" TID:")
                    .Append(cThreadId);
            }
            else if (currentThreadId != null)
            {
                formatLogLine
                    .Append(" TID:")
                    .Append(currentThreadId);
            }
            formatLogLine
                .Append(" ")
                .Append(DateTime.Now.ToString())
                .Append(" ")
                .Append(optional);
            writeStr(path, formatLogLine);
        }

        internal static void MakeErrorLog(Exception ex, string optional = null)
        {
            try
            {
                string appdata = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                string appdataQT = Path.Combine(appdata, "QTTabBar");
                if (!Directory.Exists(appdataQT))
                {
                    Directory.CreateDirectory(appdataQT);
                }
                // string path = Path.Combine(appdataQT, "QTTabBarBandObject.bandLog");
                string path = Path.Combine(appdataQT, "QTTabBarBandObjectException.log");
                using (StreamWriter writer = new StreamWriter(path, true))
                {
                    writer.WriteLine(DateTime.Now.ToString());
                    writer.WriteLine(".NET �汾: " + Environment.Version);
                    writer.WriteLine("����ϵͳ�汾: " + Environment.OSVersion.Version);
                    //writer.WriteLine("QT �汾: " + MakeVersionString());
                    if (!String.IsNullOrEmpty(optional))
                    {
                        writer.WriteLine("������Ϣ: " + optional);
                    }
                    if (ex == null)
                    {
                        writer.WriteLine("Exception: None");
                        writer.WriteLine(Environment.StackTrace);
                    }
                    else
                    {
                        // writer.WriteLine(ex.ToString());

                        writer.WriteLine("\nMessage ---\n{0}", ex.Message);
                        writer.WriteLine(
                            "\nHelpLink ---\n{0}", ex.HelpLink);
                        writer.WriteLine("\nSource ---\n{0}", ex.Source);
                        writer.WriteLine(
                            "\nStackTrace ---\n{0}", ex.StackTrace);
                        writer.WriteLine(
                            "\nTargetSite ---\n{0}", ex.TargetSite);


                    }
                    writer.WriteLine("--------------");
                    writer.WriteLine();
                    writer.Close();
                }
                // SystemSounds.Exclamation.Play();
            }
            catch
            {
            }
        }
    }
}
