/* add your code here */

#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOKit/usb/USB.h>

#include "AtherosFWService.hpp"

#define ATH3K_DNLOAD                0x01
#define ATH3K_GETSTATE                0x05
#define ATH3K_SET_NORMAL_MODE            0x07
#define ATH3K_GETVERSION            0x09
#define USB_REG_SWITCH_VID_PID            0x0a

#define ATH3K_MODE_MASK                0x3F
#define ATH3K_NORMAL_MODE            0x0E

#define ATH3K_PATCH_UPDATE            0x80
#define ATH3K_SYSCFG_UPDATE            0x40

#define FW_HDR_SIZE        20
#define BULK_SIZE           4096
#define ATH3K_XTAL_FREQ_26M            0x00
#define ATH3K_XTAL_FREQ_40M            0x01
#define ATH3K_XTAL_FREQ_19P2            0x02
#define ATH3K_NAME_LEN                0xFF

struct ath3k_version {
    __le32    rom_version;
    __le32    build_version;
    __le32    ram_version;
    __u8    ref_clock;
    __u8    reserved[7];
} __packed;

#define DRV_NAME    "Ath3kBT"

class Ath3kBT : public AtherosFWService {
    OSDeclareDefaultStructors(Ath3kBT)
    
public:
    virtual bool init(OSDictionary *dictionary = NULL) override;
    virtual void free() override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual IOService * probe(IOService *provider, SInt32 *score) override;
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice) override;
    virtual bool handleOpen(IOService *forClient, IOOptionBits options = 0, void *arg = 0 ) override;
    virtual void handleClose(IOService *forClient, IOOptionBits options = 0 ) override;
    virtual IOReturn message(UInt32 type, IOService *provider, void *argument) override;
    virtual bool terminate(IOOptionBits options = 0) override;
    virtual bool finalize(IOOptionBits options) override;
    
protected:
    
    bool setNormalMode();
    
    bool loadSysCfg();
    
    bool loadPatch();
    
    bool loadFwFile(OSData *fwData);
    
    bool switchPID(IOService* forClient);
    
    IOReturn getVendorState(IOService* forClient, unsigned char *state);
    
    IOReturn getVendorVersion(IOService* forClient, struct ath3k_version *version);
    
    IOReturn getDeviceStatus(IOService* forClient, USBStatus *status);
    
protected:
    IOUSBHostDevice * m_pUsbDevice;
    IOUSBHostPipe* m_pBulkWritePipe;
};
