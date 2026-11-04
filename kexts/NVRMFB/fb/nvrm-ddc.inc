/*
 * nvrm-ddc.inc -- DDC/CI for external monitors through the IOFramebuffer I2C interface.
 *
 * thebcd69 personal fork of NullMoth (branch ddc), not part of NullMoth and not offered upstream.
 * Licensed like the file that includes it: PolyForm Noncommercial 1.0.0.
 *
 * Why: macOS apps that set a monitor's own brightness/contrast (ASUS DisplayWidget Center, BetterDisplay's DDC mode,
 * ddcctl, MonitorControl on Intel) find the framebuffer's IOI2CInterface (IOFBCopyI2CInterfaceForBus) and send VESA
 * DDC/CI packets to address 0x6E. NVRMFB only served EDID (getDDCBlock), so those apps saw no DDC-capable display.
 *
 * How: the framebuffer publishes one I2C bus (IOFBI2CInterfaceInfo, read by IOFramebuffer::open) and answers
 * doI2CRequest by asking RM directly:
 *   - DisplayPort sinks (the USB-C/TB DP alt-mode port): NV0073_CTRL_CMD_DP_AUXCH_I2C_TRANSFER_CTRL, I2C over the
 *     AUX channel -- what NVIDIA's Linux driver does for its i2c-over-DpAux adapters (osapi.c RmDpAuxI2CTransfer).
 *   - other sinks (HDMI): NV402C_CTRL_CMD_I2C_TRANSACTION on the display's DDC port (RmNonDPAuxI2CTransfer).
 * The RM display is found by comparing RM's cached EDID (GET_EDID_V2, COPY_CACHE -- no bus traffic) with this
 * framebuffer's EDID.
 *
 * NVRM exports no RM entry to other kexts (everything goes through callPlatformFunction, and NVRM's Info.plist has
 * no OSBundleCompatibleVersion, so it cannot be linked against). Rebuilding NVRM is not possible from this tree, so
 * the three NVRM functions used here (rm_kernel_rmapi_op, nv_xnu_pm_enter/exit) are located from the one NVRM code
 * address the framebuffer is given ("phys-for-va" = nv_xnu_phys_for_kernel_va): NVRM's __text keeps its internal
 * layout in the kernel collection, so their distance from it is fixed for one NVRM build. Every address is checked
 * against the instruction bytes of NullMoth 2.7.1's NVRM (masks skip RIP-relative data and call displacements the
 * collection linker rewrites) and NVRM must report nvrm-kext-version 2.7.1. Any mismatch turns DDC off, logged in
 * the framebuffer's "nvrm-ddc" property; nothing else changes.
 *
 * Safety: only the DDC/CI address (0x37, 8-bit 0x6E/0x6F) may be written; EDID (0x50) may be read. The internal
 * panel publishes no bus. Boot-arg nvddc=0 turns the feature off.
 *
 * Debug without sudo:  ioreg -r -w0 -c NVRMNVDAFramebuffer -d1 | grep nvrm-ddc
 */

#include <IOKit/i2c/IOI2CInterface.h>
#include <kern/clock.h>
#include <stdarg.h>

/* ---- RM ABI mirrors (open-gpu-kernel-modules 615.78.08: nvos.h, nv-kernel-rmapi-ops.h, cl0073, ctrl0073*, ctrl402c) */
namespace nvddc {

enum : NvU32 {
    kNV01_FREE = 0x00, kNV04_ALLOC = 0x15, kNV04_CONTROL = 0x36,
    kClsRootClient = 0x0041, kClsDevice0 = 0x0080, kClsSubdevice0 = 0x2080, kClsDispCommon = 0x0073, kClsI2c = 0x402c,
    kCmdGpuAttachIds      = 0x00000215u,   /* NV0000_CTRL_CMD_GPU_ATTACH_IDS */
    kCmdDispGetSupported  = 0x00730107u,   /* NV0073_CTRL_CMD_SYSTEM_GET_SUPPORTED */
    kCmdDispGetEdidV2     = 0x00730245u,   /* NV0073_CTRL_CMD_SPECIFIC_GET_EDID_V2 */
    kCmdDispGetI2cPortId  = 0x00730211u,   /* NV0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID */
    kCmdDispOrGetInfo     = 0x0073028bu,   /* NV0073_CTRL_CMD_SPECIFIC_OR_GET_INFO */
    kCmdDpAuxI2cTransfer  = 0x0073137cu,   /* NV0073_CTRL_CMD_DP_AUXCH_I2C_TRANSFER_CTRL */
    kCmdI2cTransaction    = 0x402c0105u,   /* NV402C_CTRL_CMD_I2C_TRANSACTION */
    kGpuInvalidId = 0xffffffffu,
};

struct Os00 { NvU32 hRoot, hObjectParent, hObjectOld, status; };
struct Os64 { NvU32 hRoot, hObjectParent, hObjectNew, hClass; NvU64 pAllocParms __attribute__((aligned(8)));
              NvU64 pRightsRequested __attribute__((aligned(8))); NvU32 paramsSize, flags, status; };
struct Os54 { NvU32 hClient, hObject, cmd, flags; NvU64 params __attribute__((aligned(8))); NvU32 paramsSize, status; };
/* nvidia_kernel_rmapi_ops_t: NvU32 op, then a union of the NVOSxx structs (largest is far below 256 bytes) */
struct Ops { NvU32 op; NvU32 pad; union { Os00 free; Os64 alloc; Os54 control; NvU8 raw[256]; } p; };
static_assert(__builtin_offsetof(Ops, p) == 8, "rmapi ops union offset");
static_assert(__builtin_offsetof(Os64, pAllocParms) == 16 && __builtin_offsetof(Os64, status) == 40, "NVOS64 layout");
static_assert(__builtin_offsetof(Os54, params) == 16 && __builtin_offsetof(Os54, status) == 28, "NVOS54 layout");

struct AttachIds   { NvU32 gpuIds[32]; NvU32 failedId; };
struct Dev0Alloc   { NvU32 deviceId, hClientShare, hTargetClient, hTargetDevice; NvU32 flags; NvU32 pad;
                     NvU64 vaSpaceSize __attribute__((aligned(8))); NvU64 vaStartInternal __attribute__((aligned(8)));
                     NvU64 vaLimitInternal __attribute__((aligned(8))); NvU32 vaMode; NvU32 pad2; };
struct Sub0Alloc   { NvU32 subDeviceId; };
struct GetSupported{ NvU32 subDeviceInstance, displayMask, displayMaskDDC; };
struct GetEdidV2   { NvU32 subDeviceInstance, displayId, bufferSize, flags; NvU8 edidBuffer[2048]; };
struct GetI2cPort  { NvU32 subDeviceInstance, displayId, commPortId, ddcPortId; };
struct OrGetInfo   { NvU32 subDeviceInstance, displayId, index, type, protocol, ditherType, ditherAlgo, location, rootPortId,
                           dcbIndex; NvU64 vbiosAddress __attribute__((aligned(8))); NvU8 bIsLitByVbios, bIsDispDynamic; };
static_assert(sizeof(OrGetInfo) == 56, "NV0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS layout");
struct DpAuxI2c    { NvU32 subDeviceInstance, displayId, addr; NvU8 bWrite; NvU8 data[256]; NvU32 size; };
static_assert(__builtin_offsetof(DpAuxI2c, data) == 13 && __builtin_offsetof(DpAuxI2c, size) == 272 &&
              sizeof(DpAuxI2c) == 276, "NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS layout");
/* NV402C_CTRL_I2C_TRANSACTION_PARAMS with transData as NV402C_CTRL_I2C_TRANSACTION_DATA_I2C_BLOCK_RW */
struct I2cBlock    { NvU8 bWrite; NvU8 pad[3]; NvU32 messageLength; NvU64 pMessage __attribute__((aligned(8))); };
struct I2cTrans    { NvU8 portId; NvU8 pad[3]; NvU32 flags; NvU16 deviceAddress; NvU16 pad2; NvU32 transType;
                     union { I2cBlock block; NvU8 raw[80]; } transData __attribute__((aligned(8))); };
static_assert(__builtin_offsetof(I2cTrans, transData) == 16 && sizeof(I2cTrans) == 96, "NV402C_CTRL_I2C_TRANSACTION_PARAMS layout");
static_assert(sizeof(Dev0Alloc) == 56 && sizeof(GetEdidV2) == 2064 && sizeof(GetI2cPort) == 16 && sizeof(GetSupported) == 12 &&
              sizeof(AttachIds) == 132, "RM control/alloc parameter sizes (RM rejects a size mismatch)");
enum : NvU32 { kTransI2cBlockRw = 2, kEdidCopyCache = 1, kI2cPortNone = 0, kOrTypeSor = 2, kOrProtoDpA = 8, kOrProtoDpB = 9 };

/* ---- NVRM entry points, located from nv_xnu_phys_for_kernel_va (see header) ---- */
typedef void (*RmapiOpFn)(void *sp, void *ops);
typedef int  (*PmEnterFn)(void);
typedef void (*PmExitFn)(int counted);

struct Sig { long delta; unsigned len; NvU8 b[25]; NvU32 mask; /* bit i set = byte i is not compared */ };
#define NVDDC_M(lo, n) ((((NvU32)1 << (n)) - 1) << (lo))
/* NullMoth 2.7.1 NVRM (sha256 9afb717cfb481636...): phys_for_va 0x15de0, rm_kernel_rmapi_op 0x2ec470,
 * nv_xnu_pm_enter 0x5ca0, nv_xnu_pm_exit 0x5d40 (llvm-nm / llvm-objdump of /Library/Extensions/NVRM.kext). */
static const char kNvrmVersion[] = "2.7.1";
static const Sig kBase    = { 0, 16, { 0x83,0x3d,0,0,0,0,0x00,0x74,0x55,0x55,0x48,0x89,0xe5,0x48,0x8d,0x0d }, NVDDC_M(2, 4) };
static const Sig kRmapiOp = { 0x2ec470 - 0x15de0, 16,
                              { 0xf3,0x0f,0x1e,0xfa,0x55,0x48,0x89,0xe5,0x53,0x50,0x8b,0x06,0x83,0xf8,0x2d,0x7f }, 0 };
static const Sig kPmEnter = { 0x5ca0 - 0x15de0, 22,
                              { 0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x4c,0x8b,0x35,0,0,0,0,0x31,0xdb,0x4d,0x85,0xf6,0x74,0x7c,0xe8 },
                              NVDDC_M(10, 4) };
static const Sig kPmExit  = { 0x5d40 - 0x15de0, 25,
                              { 0x85,0xff,0x0f,0x95,0xc0,0x48,0x8b,0x3d,0,0,0,0,0x48,0x85,0xff,0x0f,0x95,0xc1,0x20,0xc1,0x80,0xf9,
                                0x01,0x75,0x3c }, NVDDC_M(8, 4) };
#undef NVDDC_M

static bool sigMatch(uintptr_t base, const Sig &s)
{
    const NvU8 *p = (const NvU8 *)(base + s.delta);
    for (unsigned i = 0; i < s.len; i++)
        if (!(s.mask & (1u << i)) && p[i] != s.b[i]) return false;
    return true;
}

/* One RM client for all framebuffers (client, device, subdevice, display common, lazily an NV40_I2C). */
struct State {
    IOLock   *lock = nullptr;
    bool      resolved = false, usable = false;
    char      why[96] = {};
    RmapiOpFn op = nullptr; PmEnterFn pmEnter = nullptr; PmExitFn pmExit = nullptr;
    NvU32     hClient = 0, hDevice = 0, hSub = 0, hDisp = 0, hI2c = 0;
};
static State gS;

static void zero(void *p, size_t n) { volatile NvU8 *b = (volatile NvU8 *)p; while (n--) *b++ = 0; }

static NvU32 rmAlloc(NvU32 hRoot, NvU32 hParent, NvU32 hClass, void *params, NvU32 size, NvU32 *hOut)
{
    Ops o; zero(&o, sizeof o);
    o.op = kNV04_ALLOC;
    o.p.alloc.hRoot = hRoot; o.p.alloc.hObjectParent = hParent; o.p.alloc.hClass = hClass;
    o.p.alloc.pAllocParms = (NvU64)(uintptr_t)params; o.p.alloc.paramsSize = size;
    gS.op(nullptr, &o);
    if (o.p.alloc.status == 0 && hOut) *hOut = o.p.alloc.hObjectNew;
    return o.p.alloc.status;
}
static NvU32 rmControl(NvU32 hObject, NvU32 cmd, void *params, NvU32 size)
{
    Ops o; zero(&o, sizeof o);
    o.op = kNV04_CONTROL;
    o.p.control.hClient = gS.hClient; o.p.control.hObject = hObject; o.p.control.cmd = cmd;
    o.p.control.params = (NvU64)(uintptr_t)params; o.p.control.paramsSize = size;
    gS.op(nullptr, &o);
    return o.p.control.status;
}
static void rmFreeClient()
{
    if (!gS.hClient) return;
    Ops o; zero(&o, sizeof o);
    o.op = kNV01_FREE;
    o.p.free.hRoot = gS.hClient; o.p.free.hObjectParent = gS.hClient; o.p.free.hObjectOld = gS.hClient;
    gS.op(nullptr, &o);      // freeing the root releases device, subdevice, display and I2C objects with it
    gS.hClient = gS.hDevice = gS.hSub = gS.hDisp = gS.hI2c = 0;
}

/* Find NVRM's entry points once. Called with gS.lock held. */
static bool resolve(IOService *rm, uintptr_t physForVa)
{
    if (gS.resolved) return gS.usable;
    gS.resolved = true;
    OSString *ver = rm ? OSDynamicCast(OSString, rm->getProperty("nvrm-kext-version")) : nullptr;
    if (!ver || !ver->isEqualTo(kNvrmVersion)) {
        snprintf(gS.why, sizeof gS.why, "off: NVRM %s, this build knows %s", ver ? ver->getCStringNoCopy() : "?", kNvrmVersion);
        return false;
    }
    if (!physForVa || !sigMatch(physForVa, kBase)) { snprintf(gS.why, sizeof gS.why, "off: phys-for-va does not match"); return false; }
    if (!sigMatch(physForVa, kRmapiOp) || !sigMatch(physForVa, kPmEnter) || !sigMatch(physForVa, kPmExit)) {
        snprintf(gS.why, sizeof gS.why, "off: NVRM code does not match NullMoth %s", kNvrmVersion);
        return false;
    }
    gS.op      = (RmapiOpFn)(physForVa + kRmapiOp.delta);
    gS.pmEnter = (PmEnterFn)(physForVa + kPmEnter.delta);
    gS.pmExit  = (PmExitFn)(physForVa + kPmExit.delta);
    gS.usable  = true;
    snprintf(gS.why, sizeof gS.why, "ready");
    return true;
}

/* Allocate the RM objects. Called with gS.lock held, inside the NVRM PM gate. */
static NvU32 openClient(NvU32 gpuId)
{
    if (gS.hDisp) return 0;
    NvU32 st = rmAlloc(0, 0, kClsRootClient, nullptr, 0, &gS.hClient);
    if (st || !gS.hClient) return st ? st : 0xffff0001u;
    AttachIds at; zero(&at, sizeof at); at.gpuIds[0] = gpuId; at.gpuIds[1] = kGpuInvalidId;
    if ((st = rmControl(gS.hClient, kCmdGpuAttachIds, &at, sizeof at))) goto fail;
    {
        Dev0Alloc dp; zero(&dp, sizeof dp); dp.hClientShare = gS.hClient;
        if ((st = rmAlloc(gS.hClient, gS.hClient, kClsDevice0, &dp, sizeof dp, &gS.hDevice))) goto fail;
        Sub0Alloc sp; zero(&sp, sizeof sp);
        if ((st = rmAlloc(gS.hClient, gS.hDevice, kClsSubdevice0, &sp, sizeof sp, &gS.hSub))) goto fail;
        if ((st = rmAlloc(gS.hClient, gS.hDevice, kClsDispCommon, nullptr, 0, &gS.hDisp))) goto fail;
    }
    return 0;
fail:
    rmFreeClient();
    return st;
}

} // namespace nvddc

/* ---- framebuffer side ---- */

void NVRMNVDAFramebuffer::ddcStatus(const char *fmt, ...)
{
    char s[160];
    va_list ap; va_start(ap, fmt); vsnprintf(s, sizeof s, fmt, ap); va_end(ap);
    char line[200];
    snprintf(line, sizeof line, "%s | ok %u err %u", s, (unsigned)fDdcOk, (unsigned)fDdcErr);
    setProperty("nvrm-ddc", line);
    FBLOG("fb%u: ddc: %s", fIndex, line);
}

// Called from start() before super::start(): IOFramebuffer::open() builds its IOI2CInterface nubs from this array.
void NVRMNVDAFramebuffer::publishDdc()
{
    if (!nvddc::gS.lock) {
        IOLock *l = IOLockAlloc();
        if (l && !OSCompareAndSwapPtr(nullptr, l, (void * volatile *)&nvddc::gS.lock)) IOLockFree(l);
    }
    int on = 1;
    PE_parse_boot_argn("nvddc", &on, sizeof on);
    if (!on) { setProperty("nvrm-ddc", "off (boot-arg nvddc=0)"); return; }
    if (fInternal) return;   // the laptop panel has no DDC/CI; its brightness is the backlight handler
    OSDictionary *bus = OSDictionary::withCapacity(4);
    OSArray *arr = OSArray::withCapacity(1);
    if (bus && arr) {
        OSNumber *n;
        if ((n = OSNumber::withNumber((unsigned long long)0, 32)))  { bus->setObject(kIOI2CInterfaceIDKey, n); n->release(); }
        if ((n = OSNumber::withNumber((unsigned long long)kIOI2CBusTypeI2C, 32))) { bus->setObject(kIOI2CBusTypeKey, n); n->release(); }
        const unsigned long long types = (1u << kIOI2CNoTransactionType) | (1u << kIOI2CSimpleTransactionType) |
                                         (1u << kIOI2CDDCciReplyTransactionType) | (1u << kIOI2CCombinedTransactionType);
        if ((n = OSNumber::withNumber(types, 32))) { bus->setObject(kIOI2CTransactionTypesKey, n); n->release(); }
        if ((n = OSNumber::withNumber((unsigned long long)kIOI2CUseSubAddressCommFlag, 32))) {
            bus->setObject(kIOI2CSupportedCommFlagsKey, n); n->release();
        }
        arr->setObject(bus);
        setProperty(kIOFBI2CInterfaceInfoKey, arr);
        ddcStatus("bus published, idle");
    }
    OSSafeReleaseNULL(bus); OSSafeReleaseNULL(arr);
}

// Which RM display drives this framebuffer: the one whose cached EDID is ours. Serial bytes 12..15 and the checksum
// are skipped (NVRMFB rewrites them for twin monitors, see the serial hash in the EDID take-over).
NvU32 NVRMNVDAFramebuffer::ddcFindDisplay()
{
    using namespace nvddc;
    if (fEdidSize < 128) return 0;
    GetSupported gs; zero(&gs, sizeof gs);
    NvU32 st = rmControl(gS.hDisp, kCmdDispGetSupported, &gs, sizeof gs);
    if (st) { ddcStatus("GET_SUPPORTED failed 0x%x", (unsigned)st); return 0; }
    GetEdidV2 *e = (GetEdidV2 *)IOMalloc(sizeof(GetEdidV2));
    if (!e) return 0;
    NvU32 hit = 0;
    for (NvU32 m = gs.displayMask; m && !hit; m &= m - 1) {
        const NvU32 id = m & (0u - m);
        zero(e, sizeof *e);
        e->displayId = id; e->bufferSize = sizeof e->edidBuffer; e->flags = kEdidCopyCache;
        if (rmControl(gS.hDisp, kCmdDispGetEdidV2, e, sizeof *e) || e->bufferSize < 128) continue;
        if (memcmp(e->edidBuffer, fEdid, 12) == 0 && memcmp(e->edidBuffer + 16, fEdid + 16, 111) == 0) hit = id;
    }
    IOFree(e, sizeof(GetEdidV2));
    if (!hit) ddcStatus("no RM display has this EDID (mask 0x%x)", (unsigned)gs.displayMask);
    return hit;
}

// How to reach the display found by ddcFindDisplay: DisplayPort (an SOR running DP) over AUX, anything else through its
// DDC port. Called with gS.lock held, inside the PM gate.
bool NVRMNVDAFramebuffer::ddcSetupPath()
{
    using namespace nvddc;
    OrGetInfo oi; zero(&oi, sizeof oi); oi.displayId = fDdcDisplayId;
    NvU32 st = rmControl(gS.hDisp, kCmdDispOrGetInfo, &oi, sizeof oi);
    if (st == 0 && oi.type == kOrTypeSor && (oi.protocol == kOrProtoDpA || oi.protocol == kOrProtoDpB)) {
        fDdcMode = 1;
        ddcStatus("display 0x%x: DisplayPort, I2C over AUX", (unsigned)fDdcDisplayId);
        return true;
    }
    GetI2cPort gp; zero(&gp, sizeof gp); gp.displayId = fDdcDisplayId;
    NvU32 s2 = rmControl(gS.hDisp, kCmdDispGetI2cPortId, &gp, sizeof gp);
    if (s2 || gp.ddcPortId == kI2cPortNone) {
        ddcStatus("display 0x%x: OR 0x%x type %u protocol %u, no DDC port (0x%x)", (unsigned)fDdcDisplayId, (unsigned)st,
                  (unsigned)oi.type, (unsigned)oi.protocol, (unsigned)s2);
        return false;
    }
    if (!gS.hI2c && (s2 = rmAlloc(gS.hClient, gS.hSub, kClsI2c, nullptr, 0, &gS.hI2c))) {
        ddcStatus("NV40_I2C alloc failed 0x%x", (unsigned)s2);
        return false;
    }
    fDdcPort = gp.ddcPortId - 1; fDdcMode = 2;
    ddcStatus("display 0x%x: protocol %u, DDC port %u", (unsigned)fDdcDisplayId, (unsigned)oi.protocol, (unsigned)fDdcPort);
    return true;
}

// One raw I2C transfer to 7-bit address `addr`. Called with gS.lock held, inside the PM gate, path set up.
IOReturn NVRMNVDAFramebuffer::ddcXfer(NvU8 addr, bool write, NvU8 *buf, NvU32 len)
{
    using namespace nvddc;
    if (len == 0) return kIOReturnSuccess;
    if (len > 256) return kIOReturnBadArgument;
    NvU32 st;
    if (fDdcMode == 1) {
        DpAuxI2c *p = (DpAuxI2c *)IOMalloc(sizeof(DpAuxI2c));
        if (!p) return kIOReturnNoMemory;
        zero(p, sizeof *p);
        p->displayId = fDdcDisplayId; p->addr = addr; p->bWrite = write; p->size = len;
        if (write) memcpy(p->data, buf, len);
        st = rmControl(gS.hDisp, kCmdDpAuxI2cTransfer, p, sizeof *p);
        if (st == 0 && !write) memcpy(buf, p->data, p->size < len ? p->size : len);
        IOFree(p, sizeof(DpAuxI2c));
    } else if (fDdcMode == 2) {
        I2cTrans t; zero(&t, sizeof t);
        t.portId = (NvU8)fDdcPort; t.deviceAddress = (NvU16)(addr << 1); t.transType = kTransI2cBlockRw;
        t.transData.block.bWrite = write; t.transData.block.messageLength = len;
        t.transData.block.pMessage = (NvU64)(uintptr_t)buf;
        st = rmControl(gS.hI2c, kCmdI2cTransaction, &t, sizeof t);
    } else return kIOReturnNoDevice;
    if (st) { fDdcLastStatus = st; return kIOReturnIOError; }
    return kIOReturnSuccess;
}

IOReturn NVRMNVDAFramebuffer::doI2CRequest(UInt32 bus, IOI2CBusTiming *timing, IOI2CRequest *req)
{
    using namespace nvddc;
    if (!req) return kIOReturnBadArgument;
    IOReturn err = kIOReturnSuccess;
    const NvU8 sendA = (NvU8)(req->sendAddress >> 1), replyA = (NvU8)(req->replyAddress >> 1);
    const bool sub = (req->commFlags & kIOI2CUseSubAddressCommFlag) != 0;
    const bool doSend = req->sendTransactionType != kIOI2CNoTransactionType;
    const bool doReply = req->replyTransactionType != kIOI2CNoTransactionType;

    do {
        if (bus != 0 || fInternal || fDpyLost) { err = kIOReturnNoDevice; break; }
        if ((doSend && (req->sendTransactionType > kIOI2CCombinedTransactionType || (req->sendAddress & 1))) ||
            (doReply && req->replyTransactionType > kIOI2CCombinedTransactionType)) { err = kIOReturnUnsupportedMode; break; }
        // Only DDC/CI may be written; DDC/CI and EDID may be read.
        if ((doSend && sendA != 0x37) || (doReply && replyA != 0x37 && replyA != 0x50)) { err = kIOReturnNotPermitted; break; }
        if ((doSend && (!req->sendBuffer || req->sendBytes > 255)) || (doReply && (!req->replyBuffer || req->replyBytes > 256)))
            { err = kIOReturnBadArgument; break; }

        IOService *rm = getProvider();
        while (rm && strcmp(rm->getMetaClass()->getClassName(), "NVRM") != 0) rm = rm->getProvider();
        const OSSymbol *ready = OSSymbol::withCString("NVRMGpuReady");
        const IOReturn rr = (rm && ready) ? rm->callPlatformFunction(ready, false, NULL, NULL, NULL, NULL) : kIOReturnNotFound;
        OSSafeReleaseNULL(ready);
        if (rr != kIOReturnSuccess) { err = kIOReturnOffline; break; }

        if (!gS.lock) { err = kIOReturnNotReady; break; }
        IOLockLock(gS.lock);
        if (!resolve(rm, (uintptr_t)fPhysForVa)) {
            IOLockUnlock(gS.lock);
            if (!fDdcSaidOff) { fDdcSaidOff = true; ddcStatus("%s", gS.why); }
            err = kIOReturnUnsupported; break;
        }
        const int counted = gS.pmEnter();
        NvU32 st = openClient(fGpuId);
        if (st) {
            ddcStatus("RM client setup failed 0x%x", (unsigned)st);
            err = kIOReturnNoResources;
        } else {
            // EDID changed (another monitor on this connector): find the RM display again
            NvU32 key = 0; for (unsigned i = 8; i < 12 && i < fEdidSize; i++) key = (key << 8) | fEdid[i];
            if (fDdcEdidKey != key) { fDdcEdidKey = key; fDdcDisplayId = 0; fDdcMode = 0; }
            if (!fDdcDisplayId) { fDdcDisplayId = ddcFindDisplay(); fDdcMode = 0; }
            if (fDdcDisplayId && !fDdcMode && !ddcSetupPath()) fDdcDisplayId = 0;
            if (!fDdcDisplayId) err = kIOReturnNoDevice;
        }
        if (err == kIOReturnSuccess && doSend) {
            NvU8 tmp[256];
            NvU32 n = 0;
            if (sub) tmp[n++] = req->sendSubAddress;
            memcpy(tmp + n, (const void *)req->sendBuffer, req->sendBytes); n += req->sendBytes;
            err = ddcXfer(sendA, true, tmp, n);
        }
        if (err == kIOReturnSuccess && doReply) {
            if (doSend) {
                // DDC/CI needs >= 40 ms between a request and reading its reply; honour a longer minReplyDelay
                uint64_t ns = 0; absolutetime_to_nanoseconds(req->minReplyDelay, &ns);
                uint32_t ms = (uint32_t)(ns / 1000000ull);
                if (ms < 40 && replyA == 0x37) ms = 40;
                if (ms > 500) ms = 500;
                if (ms) IOSleep(ms);
            }
            if (sub && !doSend) {     // a read at a subaddress: write the subaddress first
                NvU8 sa = req->replySubAddress;
                err = ddcXfer(replyA, true, &sa, 1);
            }
            if (err == kIOReturnSuccess)
                err = ddcXfer(replyA, false, (NvU8 *)req->replyBuffer, req->replyBytes);
        }
        gS.pmExit(counted);
        IOLockUnlock(gS.lock);
        if (err == kIOReturnSuccess) {
            if (fDdcOk++ == 0) ddcStatus("display 0x%x %s: first transfer ok", (unsigned)fDdcDisplayId,
                                         fDdcMode == 2 ? "DDC port" : "AUX");
        } else if (err == kIOReturnIOError) {
            if ((fDdcErr++ & 0x3f) == 0) ddcStatus("transfer to 0x%02x failed, RM 0x%x", (unsigned)(doSend ? sendA : replyA),
                                                   (unsigned)fDdcLastStatus);
        }
    } while (false);

    req->result = err;
    if (req->completion) (*req->completion)(req);
    return kIOReturnSuccess;
}
