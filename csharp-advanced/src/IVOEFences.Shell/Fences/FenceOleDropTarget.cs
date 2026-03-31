using IVOEFences.Shell.Native;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text;

namespace IVOEFences.Shell.Fences;

[ComVisible(true)]
[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
[Guid("00000122-0000-0000-C000-000000000046")]
internal interface IOleDropTarget
{
    [PreserveSig]
    int DragEnter(IDataObject pDataObj, int grfKeyState, POINTL pt, ref int pdwEffect);

    [PreserveSig]
    int DragOver(int grfKeyState, POINTL pt, ref int pdwEffect);

    [PreserveSig]
    int DragLeave();

    [PreserveSig]
    int Drop(IDataObject pDataObj, int grfKeyState, POINTL pt, ref int pdwEffect);
}

[StructLayout(LayoutKind.Sequential)]
internal struct POINTL
{
    public int x;
    public int y;
}

[ComVisible(true)]
[ClassInterface(ClassInterfaceType.None)]
internal sealed class FenceOleDropTarget : IOleDropTarget
{
    private const int S_OK = 0;
    private const int DV_E_FORMATETC = unchecked((int)0x80040064);
    private const int TYMED_HGLOBAL = 1;
    private const int CF_HDROP = 15;
    private const int DROPEFFECT_COPY = 1;
    private const int DROPEFFECT_MOVE = 2;

    private readonly FenceWindow _owner;

    public FenceOleDropTarget(FenceWindow owner)
    {
        _owner = owner;
    }

    public int DragEnter(IDataObject pDataObj, int grfKeyState, POINTL pt, ref int pdwEffect)
    {
        pdwEffect = DROPEFFECT_MOVE | DROPEFFECT_COPY;
        _owner.SetExternalDropHover(true);
        return S_OK;
    }

    public int DragOver(int grfKeyState, POINTL pt, ref int pdwEffect)
    {
        pdwEffect = DROPEFFECT_MOVE | DROPEFFECT_COPY;
        return S_OK;
    }

    public int DragLeave()
    {
        _owner.SetExternalDropHover(false);
        return S_OK;
    }

    public int Drop(IDataObject pDataObj, int grfKeyState, POINTL pt, ref int pdwEffect)
    {
        pdwEffect = DROPEFFECT_MOVE;
        _owner.SetExternalDropHover(false);

        var files = GetDroppedFiles(pDataObj);
        if (files.Count > 0)
            _owner.HandleOleDroppedFiles(files, new Win32.POINT { x = pt.x, y = pt.y });

        return S_OK;
    }

    private static List<string> GetDroppedFiles(IDataObject dataObj)
    {
        var result = new List<string>();

        var format = new FORMATETC
        {
            cfFormat = CF_HDROP,
            dwAspect = DVASPECT.DVASPECT_CONTENT,
            lindex = -1,
            tymed = TYMED.TYMED_HGLOBAL,
            ptd = IntPtr.Zero,
        };

        try
        {
            dataObj.GetData(ref format, out STGMEDIUM medium);

            try
            {
                IntPtr hDrop = medium.unionmember;
                if (hDrop == IntPtr.Zero)
                    return result;

                uint count = Shell32.DragQueryFile(hDrop, 0xFFFFFFFF, null, 0);
                for (uint i = 0; i < count; i++)
                {
                    var sb = new StringBuilder(1024);
                    uint len = Shell32.DragQueryFile(hDrop, i, sb, (uint)sb.Capacity);
                    if (len > 0)
                        result.Add(sb.ToString());
                }
            }
            finally
            {
                ReleaseStgMedium(ref medium);
            }
        }
        catch (COMException ex) when (ex.ErrorCode == DV_E_FORMATETC)
        {
            // Not a file drop payload; ignore.
        }

        return result;
    }

    [DllImport("ole32.dll")]
    private static extern void ReleaseStgMedium(ref STGMEDIUM pmedium);
}
