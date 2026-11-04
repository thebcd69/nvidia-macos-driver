// ddcprobe -- read (and optionally set) a VESA MCCS VCP code over the framebuffer I2C interface, like ddcctl.
// thebcd69 fork (branch ddc) test tool for the NVRMFB DDC/CI path.
//   ddcprobe                 list framebuffers with I2C buses, read brightness (VCP 0x10) on each
//   ddcprobe <fb> <vcp>      read VCP code (hex) on framebuffer index <fb> (IOFBDependentIndex)
//   ddcprobe <fb> <vcp> <v>  set it to <v> (decimal), then read it back
// Build: clang -O2 -framework IOKit -framework CoreFoundation ddcprobe.c -o ddcprobe
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fbIndex(io_service_t fb)
{
    CFTypeRef v = IORegistryEntryCreateCFProperty(fb, CFSTR("IOFBDependentIndex"), kCFAllocatorDefault, 0);
    int idx = -1;
    if (v && CFGetTypeID(v) == CFNumberGetTypeID()) CFNumberGetValue((CFNumberRef)v, kCFNumberIntType, &idx);
    if (v) CFRelease(v);
    return idx;
}

static IOReturn xfer(io_service_t fb, UInt8 *send, int nsend, UInt8 *reply, int nreply)
{
    io_service_t bus = 0;
    IOReturn r = IOFBCopyI2CInterfaceForBus(fb, 0, &bus);
    if (r) return r;
    IOI2CConnectRef c = NULL;
    r = IOI2CInterfaceOpen(bus, kNilOptions, &c);
    IOObjectRelease(bus);
    if (r) return r;
    IOI2CRequest q;
    memset(&q, 0, sizeof q);
    q.sendTransactionType = kIOI2CSimpleTransactionType;
    q.sendAddress = 0x6E;
    q.sendBuffer = (vm_address_t)send;
    q.sendBytes = nsend;
    if (nreply) {
        q.replyTransactionType = kIOI2CDDCciReplyTransactionType;
        q.replyAddress = 0x6F;
        q.replyBuffer = (vm_address_t)reply;
        q.replyBytes = nreply;
        q.minReplyDelay = 50 * 1000 * 1000;   // ns (absolute time on x86)
    }
    r = IOI2CSendRequest(c, kNilOptions, &q);
    IOI2CInterfaceClose(c, kNilOptions);
    return r ? r : q.result;
}

static int getVcp(io_service_t fb, UInt8 vcp, int *cur, int *max)
{
    UInt8 s[5] = { 0x51, 0x82, 0x01, vcp, 0 };
    s[4] = 0x6E ^ s[0] ^ s[1] ^ s[2] ^ s[3];
    UInt8 r[11];
    for (int tries = 0; tries < 3; tries++) {
        memset(r, 0, sizeof r);
        IOReturn e = xfer(fb, s, 5, r, sizeof r);
        if (e) { fprintf(stderr, "  read VCP 0x%02x: IOReturn 0x%x\n", vcp, e); usleep(100000); continue; }
        UInt8 chk = 0x50; for (int i = 0; i < 10; i++) chk ^= r[i];
        printf("  reply:"); for (int i = 0; i < 11; i++) printf(" %02x", r[i]); printf("%s\n", chk == r[10] ? "" : "  (checksum mismatch)");
        if (r[2] == 0x02 && r[4] == vcp && r[3] == 0) { *max = (r[6] << 8) | r[7]; *cur = (r[8] << 8) | r[9]; return 0; }
        usleep(100000);
    }
    return -1;
}

static int setVcp(io_service_t fb, UInt8 vcp, int v)
{
    UInt8 s[7] = { 0x51, 0x84, 0x03, vcp, (UInt8)(v >> 8), (UInt8)v, 0 };
    s[6] = 0x6E ^ s[0] ^ s[1] ^ s[2] ^ s[3] ^ s[4] ^ s[5];
    IOReturn e = xfer(fb, s, 7, NULL, 0);
    if (e) fprintf(stderr, "  set VCP 0x%02x: IOReturn 0x%x\n", vcp, e);
    return e ? -1 : 0;
}

int main(int argc, char **argv)
{
    int want = argc > 1 ? atoi(argv[1]) : -1;
    UInt8 vcp = argc > 2 ? (UInt8)strtol(argv[2], NULL, 16) : 0x10;
    io_iterator_t it;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOFramebuffer"), &it)) return 1;
    io_service_t fb; int found = 0, rc = 0;
    while ((fb = IOIteratorNext(it))) {
        int idx = fbIndex(fb);
        io_service_t bus = 0;
        IOReturn r = IOFBCopyI2CInterfaceForBus(fb, 0, &bus);
        if (bus) IOObjectRelease(bus);
        if (want < 0 || idx == want) {
            printf("framebuffer %d: I2C bus 0 %s\n", idx, r ? "absent" : "present");
            if (!r) {
                found++;
                int cur = -1, max = -1;
                if (argc > 3) { if (setVcp(fb, vcp, atoi(argv[3]))) rc = 2; usleep(200000); }
                if (getVcp(fb, vcp, &cur, &max) == 0) printf("  VCP 0x%02x = %d / %d\n", vcp, cur, max);
                else { printf("  VCP 0x%02x: no valid reply\n", vcp); rc = 3; }
            }
        }
        IOObjectRelease(fb);
    }
    IOObjectRelease(it);
    if (!found) { printf("no framebuffer with an I2C bus\n"); return 4; }
    return rc;
}
