// abi-check.cpp -- compile-time check that the RM ABI mirrors in kexts/NVRMFB/fb/nvrm-ddc.inc match NVIDIA's own
// headers (open-gpu-kernel-modules 615.78.08). thebcd69 fork, branch ddc-ci-claude. Host build only (no kernel):
//   tools/ddc/abi-check.sh <open-gpu-kernel-modules checkout>
// A mismatch is a compile error naming the field. RM rejects a wrong paramsSize, but a field at the wrong offset
// inside a correctly sized struct would be sent as-is, so every field the driver reads or writes is checked.
#include <stddef.h>
extern "C" {
#include "nvtypes.h"
#include "nvos.h"
#include "nv-kernel-rmapi-ops.h"
#include "class/cl0073.h"
#include "class/cl402c.h"
#include "ctrl/ctrl0000/ctrl0000gpu.h"
#include "ctrl/ctrl0073/ctrl0073system.h"
#include "ctrl/ctrl0073/ctrl0073specific.h"
#include "ctrl/ctrl0073/ctrl0073dp.h"
#include "ctrl/ctrl402c.h"
#include "class/cl0080.h"
#include "class/cl2080.h"
}
#include "nvddc-mirror.h"   // extracted from nvrm-ddc.inc by abi-check.sh

#define SAME(M, R, mf, rf) static_assert(offsetof(M, mf) == offsetof(R, rf), #M "." #mf " offset != " #R "." #rf)
#define SIZE(M, R) static_assert(sizeof(M) == sizeof(R), "sizeof " #M " != sizeof " #R)
using namespace nvddc;

// nvidia_kernel_rmapi_ops_t: the driver's Ops must start like it and be at least as large as the members it uses
static_assert(offsetof(Ops, p) == offsetof(nvidia_kernel_rmapi_ops_t, params), "Ops.p offset");
static_assert(offsetof(Ops, p.control) == offsetof(nvidia_kernel_rmapi_ops_t, params.control), "Ops.p.control");
static_assert(sizeof(Ops) >= offsetof(nvidia_kernel_rmapi_ops_t, params) + sizeof(NVOS54_PARAMETERS), "Ops too small for NVOS54");
static_assert(sizeof(Ops) >= offsetof(nvidia_kernel_rmapi_ops_t, params) + sizeof(NVOS64_PARAMETERS), "Ops too small for NVOS64");
static_assert(sizeof(Ops) >= offsetof(nvidia_kernel_rmapi_ops_t, params) + sizeof(NVOS00_PARAMETERS), "Ops too small for NVOS00");

SIZE(Os00, NVOS00_PARAMETERS);
SAME(Os00, NVOS00_PARAMETERS, hRoot, hRoot); SAME(Os00, NVOS00_PARAMETERS, hObjectParent, hObjectParent);
SAME(Os00, NVOS00_PARAMETERS, hObjectOld, hObjectOld); SAME(Os00, NVOS00_PARAMETERS, status, status);
SIZE(Os64, NVOS64_PARAMETERS);
SAME(Os64, NVOS64_PARAMETERS, hRoot, hRoot); SAME(Os64, NVOS64_PARAMETERS, hObjectParent, hObjectParent);
SAME(Os64, NVOS64_PARAMETERS, hObjectNew, hObjectNew); SAME(Os64, NVOS64_PARAMETERS, hClass, hClass);
SAME(Os64, NVOS64_PARAMETERS, pAllocParms, pAllocParms); SAME(Os64, NVOS64_PARAMETERS, pRightsRequested, pRightsRequested);
SAME(Os64, NVOS64_PARAMETERS, paramsSize, paramsSize); SAME(Os64, NVOS64_PARAMETERS, flags, flags);
SAME(Os64, NVOS64_PARAMETERS, status, status);
SIZE(Os54, NVOS54_PARAMETERS);
SAME(Os54, NVOS54_PARAMETERS, hClient, hClient); SAME(Os54, NVOS54_PARAMETERS, hObject, hObject);
SAME(Os54, NVOS54_PARAMETERS, cmd, cmd); SAME(Os54, NVOS54_PARAMETERS, flags, flags);
SAME(Os54, NVOS54_PARAMETERS, params, params); SAME(Os54, NVOS54_PARAMETERS, paramsSize, paramsSize);
SAME(Os54, NVOS54_PARAMETERS, status, status);

SIZE(AttachIds, NV0000_CTRL_GPU_ATTACH_IDS_PARAMS);
SAME(AttachIds, NV0000_CTRL_GPU_ATTACH_IDS_PARAMS, gpuIds, gpuIds);
SAME(AttachIds, NV0000_CTRL_GPU_ATTACH_IDS_PARAMS, failedId, failedId);
SIZE(Dev0Alloc, NV0080_ALLOC_PARAMETERS);
SAME(Dev0Alloc, NV0080_ALLOC_PARAMETERS, hClientShare, hClientShare);
SAME(Dev0Alloc, NV0080_ALLOC_PARAMETERS, vaMode, vaMode);
SIZE(Sub0Alloc, NV2080_ALLOC_PARAMETERS);
SIZE(GetSupported, NV0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS);
SAME(GetSupported, NV0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS, displayMask, displayMask);
SIZE(GetEdidV2, NV0073_CTRL_SPECIFIC_GET_EDID_V2_PARAMS);
SAME(GetEdidV2, NV0073_CTRL_SPECIFIC_GET_EDID_V2_PARAMS, displayId, displayId);
SAME(GetEdidV2, NV0073_CTRL_SPECIFIC_GET_EDID_V2_PARAMS, bufferSize, bufferSize);
SAME(GetEdidV2, NV0073_CTRL_SPECIFIC_GET_EDID_V2_PARAMS, flags, flags);
SAME(GetEdidV2, NV0073_CTRL_SPECIFIC_GET_EDID_V2_PARAMS, edidBuffer, edidBuffer);
SIZE(GetI2cPort, NV0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS);
SAME(GetI2cPort, NV0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS, displayId, displayId);
SAME(GetI2cPort, NV0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS, ddcPortId, ddcPortId);
SIZE(OrGetInfo, NV0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS);
SAME(OrGetInfo, NV0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS, displayId, displayId);
SAME(OrGetInfo, NV0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS, type, type);
SAME(OrGetInfo, NV0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS, protocol, protocol);
SAME(OrGetInfo, NV0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS, bIsDispDynamic, bIsDispDynamic);
SIZE(DpAuxI2c, NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS);
SAME(DpAuxI2c, NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS, displayId, displayId);
SAME(DpAuxI2c, NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS, addr, addr);
SAME(DpAuxI2c, NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS, bWrite, bWrite);
SAME(DpAuxI2c, NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS, data, data);
SAME(DpAuxI2c, NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_CTRL_PARAMS, size, size);
static_assert(sizeof(((DpAuxI2c *)0)->data) == NV0073_CTRL_DP_AUXCH_I2C_TRANSFER_MAX_DATA_SIZE, "DpAuxI2c.data size");
SIZE(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, portId, portId);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, flags, flags);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, deviceAddress, deviceAddress);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, transType, transType);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, transData, transData);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, transData.block.bWrite, transData.i2cBlockData.bWrite);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, transData.block.messageLength, transData.i2cBlockData.messageLength);
SAME(I2cTrans, NV402C_CTRL_I2C_TRANSACTION_PARAMS, transData.block.pMessage, transData.i2cBlockData.pMessage);

// command, class and constant values
static_assert(kNV01_FREE == NV01_FREE && kNV04_ALLOC == NV04_ALLOC && kNV04_CONTROL == NV04_CONTROL, "rmapi op codes");
static_assert(kClsRootClient == NV01_ROOT_CLIENT && kClsDevice0 == NV01_DEVICE_0 && kClsSubdevice0 == NV20_SUBDEVICE_0 &&
              kClsDispCommon == NV04_DISPLAY_COMMON && kClsI2c == NV40_I2C, "class ids");
static_assert(kCmdGpuAttachIds == NV0000_CTRL_CMD_GPU_ATTACH_IDS, "ATTACH_IDS");
static_assert(kCmdDispGetSupported == NV0073_CTRL_CMD_SYSTEM_GET_SUPPORTED, "GET_SUPPORTED");
static_assert(kCmdDispGetEdidV2 == NV0073_CTRL_CMD_SPECIFIC_GET_EDID_V2, "GET_EDID_V2");
static_assert(kCmdDispGetI2cPortId == NV0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID, "GET_I2C_PORTID");
static_assert(kCmdDispOrGetInfo == NV0073_CTRL_CMD_SPECIFIC_OR_GET_INFO, "OR_GET_INFO");
static_assert(kCmdDpAuxI2cTransfer == NV0073_CTRL_CMD_DP_AUXCH_I2C_TRANSFER_CTRL, "DP_AUXCH_I2C_TRANSFER_CTRL");
static_assert(kCmdI2cTransaction == NV402C_CTRL_CMD_I2C_TRANSACTION, "I2C_TRANSACTION");
static_assert(kTransI2cBlockRw == NV402C_CTRL_I2C_TRANSACTION_TYPE_I2C_BLOCK_RW, "I2C_BLOCK_RW");
static_assert(kI2cPortNone == NV0073_CTRL_SPECIFIC_I2C_PORT_NONE, "I2C_PORT_NONE");
static_assert(kOrTypeSor == NV0073_CTRL_SPECIFIC_OR_TYPE_SOR, "OR_TYPE_SOR");
static_assert(kOrProtoDpA == NV0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A && kOrProtoDpB == NV0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B, "DP protocols");
static_assert(kEdidCopyCache == (1u << DRF_SHIFT(NV0073_CTRL_SPECIFIC_GET_EDID_FLAGS_COPY_CACHE)), "EDID COPY_CACHE bit");

int main() { return 0; }
