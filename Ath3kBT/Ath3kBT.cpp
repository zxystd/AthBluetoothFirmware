#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>

#include "Ath3kBT.hpp"
#include "FwData.h"

enum { kMyOffPowerState = 0, kMyOnPowerState = 1 };
#define kIOPMPowerOff 0

static IOPMPowerState myTwoStates[2] =
{
    {1, kIOPMPowerOff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

#define super AtherosFWService
OSDefineMetaClassAndStructors(Ath3kBT, AtherosFWService)

bool Ath3kBT::init(OSDictionary *propTable)
{
    IOLog("%s::probe\n", getName());
    return (super::init(propTable));
}

IOService* Ath3kBT::probe(IOService *provider, SInt32 *score)
{
    IOLog("%s::probe\n", getName());
    super::probe(provider, score);
    return this;
}

bool Ath3kBT::start(IOService *provider)
{
    IOReturn                 err;
    const StandardUSB::ConfigurationDescriptor* cd;
    
    IOLog("%s::start!\n", getName());
    m_pUsbDevice = OSDynamicCast(IOUSBHostDevice, provider);
    if(!m_pUsbDevice)
    {
        IOLog("%s::start - Provider isn't a USB device!!!\n", getName());
        return false;
    }

    err = m_pUsbDevice->setConfiguration(0);
    if (err) {
        IOLog("%s::start - failed to reset the device\n", getName());
        return false;
    }
    else {
        IOLog("%s::start: device reset\n", getName());
    }
    
    int numconf = 0;
    if ((numconf = m_pUsbDevice->getDeviceDescriptor()->bNumConfigurations) < 1)
    {
        IOLog("%s::start - no composite configurations\n", getName());
        return false;
    }
    IOLog("%s::start: num configurations %d\n", getName(), numconf);
        
    cd = m_pUsbDevice->getConfigurationDescriptor(0);
    if (!cd)
    {
        IOLog("%s::start - no config descriptor\n", getName());
        return false;
    }
    
    if (!m_pUsbDevice->open(this))
    {
        IOLog("%s::start - unable to open device for configuration\n", getName());
        return false;
    }
    
    err = m_pUsbDevice->setConfiguration(cd->bConfigurationValue, true);
    if (err)
    {
        IOLog("%s::start - unable to set the configuration\n", getName());
        m_pUsbDevice->close(this);
        return false;
    }
    
    USBStatus status;
    err = getDeviceStatus(this, &status);
    if (err)
    {
        IOLog("%s::start - unable to get device status\n", getName());
        m_pUsbDevice->close(this);
        return false;
    }
    
    IOUSBHostInterface * intf = NULL;
    OSIterator* iterator = m_pUsbDevice->getChildIterator(gIOServicePlane);
    
    if (!iterator)
        return false;
    
    while (OSObject* candidate = iterator->getNextObject())
    {
        if (IOUSBHostInterface* interface = OSDynamicCast(IOUSBHostInterface, candidate))
        {
            {
                intf = interface;
                break;
            }
        }
    }
    
    iterator->release();
    if (!intf) {
        IOLog("%s::start - unable to find interface\n", getName());
        m_pUsbDevice->close(this);
        return false;
    }

    if (!intf->open(this))
    {
        IOLog("%s::start - unable to open interface\n", getName());
        m_pUsbDevice->close(this);
        return false;
    }
    
    const StandardUSB::ConfigurationDescriptor *configDescriptor = intf->getConfigurationDescriptor();
    const StandardUSB::InterfaceDescriptor *interfaceDescriptor = intf->getInterfaceDescriptor();
    if (configDescriptor == NULL || interfaceDescriptor == NULL) {
        IOLog("%s::start find descriptor NULL\n", getName());
        return false;
    }
    const EndpointDescriptor *endpointDescriptor = NULL;
    while ((endpointDescriptor = StandardUSB::getNextEndpointDescriptor(configDescriptor, interfaceDescriptor, endpointDescriptor))) {
        uint8_t epDirection = StandardUSB::getEndpointDirection(endpointDescriptor);
        uint8_t epType = StandardUSB::getEndpointType(endpointDescriptor);
        uint8_t epNum = StandardUSB::getEndpointNumber(endpointDescriptor);
        if (epDirection == kUSBOut && epType == kUSBBulk && epNum == 2) {
            IOLog("%s::start Found Bulk out endpoint!\n", getName());
            m_pBulkWritePipe = intf->copyPipe(StandardUSB::getEndpointAddress(endpointDescriptor));
            if (m_pBulkWritePipe == NULL) {
                IOLog("%s::start copy Bulk pipe fail\n", getName());
                return false;
            }
            m_pBulkWritePipe->retain();
            m_pBulkWritePipe->release();
        }
    }
    
    PMinit();
    registerPowerDriver(this, myTwoStates, 2);
    provider->joinPMtree(this);
    makeUsable();
    
    if (!loadPatch()) {
        IOLog("%s::start Loading patch file failed\n", getName());
        goto done;
    }
    
    if (!loadSysCfg()) {
        IOLog("%s::start Loading sysconfig file failed\n", getName());
        goto done;
    }
    
    if (!setNormalMode()) {
        IOLog("%s::start Set normal mode failed\n", getName());
        goto done;
    }
    
//    switchPID(this);
    
    IOLog("%s: firmware loaded successfully!\n", getName());

    err = getDeviceStatus(this, &status);
    IOLog("%s::start: device status %d\n", getName(), (int)status);
    
done:
    
    m_pBulkWritePipe->release();
    intf->close(this);
    m_pUsbDevice->close(this);
    return true;
}

void Ath3kBT::stop(IOService *provider)
{
    IOLog("%s::stop\n", getName());
    PMstop();
    super::stop(provider);
}

void Ath3kBT::free()
{
    IOLog("%s::free\n", getName());
    PMstop();
    super::free();
}

IOReturn Ath3kBT::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{
//    IOLog("%s::setPowerState powerStateOrdinal=%lu\n", powerStateOrdinal, getName());
    return IOPMAckImplied;
}


bool Ath3kBT::handleOpen(IOService *forClient, IOOptionBits options, void *arg )
{
    IOLog("%s::handleOpen\n", getName());
    return super::handleOpen(forClient, options, arg);
}

void Ath3kBT::handleClose(IOService *forClient, IOOptionBits options )
{
    IOLog("%s::handleClose\n", getName());
    super::handleClose(forClient, options);
}

IOReturn Ath3kBT::message(UInt32 type, IOService *provider, void *argument)
{
    IOLog("%s::message\n", getName());
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            if (m_pUsbDevice != NULL && m_pUsbDevice->isOpen(this))
            {
                IOLog("%s::message - service is terminated - closing device\n", getName());
            }
            break;
            
        case kIOMessageServiceIsSuspended:
        case kIOMessageServiceIsResumed:
        case kIOMessageServiceIsRequestingClose:
        case kIOMessageServiceWasClosed:
        case kIOMessageServiceBusyStateChange:
        default:
            break;
    }
    
    return super::message(type, provider, argument);
}

bool Ath3kBT::terminate(IOOptionBits options)
{
    IOLog("%s::terminate\n", getName());
    return super::terminate(options);
}

bool Ath3kBT::finalize(IOOptionBits options)
{
    IOLog("%s::finalize\n", getName());
    return super::finalize(options);
}

IOReturn Ath3kBT::getDeviceStatus(IOService* forClient, USBStatus *status)
{
    uint16_t stat       = 0;
    StandardUSB::DeviceRequest request;
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeStandard, kRequestRecipientDevice);
    request.bRequest      = kDeviceRequestGetStatus;
    request.wValue        = 0;
    request.wIndex        = 0;
    request.wLength       = sizeof(stat);
    uint32_t bytesTransferred = 0;
    IOReturn result = m_pUsbDevice->deviceRequest(forClient, request, &stat, bytesTransferred, kUSBHostStandardRequestCompletionTimeout);
    *status = stat;
    return result;
}

IOReturn Ath3kBT::getVendorState(IOService *forClient, unsigned char *state)
{
    char buf;
    StandardUSB::DeviceRequest request;
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeVendor, kRequestRecipientDevice);
    request.bRequest      = ATH3K_GETSTATE;
    request.wValue        = 0;
    request.wIndex        = 0;
    request.wLength       = sizeof(char);
    uint32_t bytesTransferred = 0;
    IOReturn result = m_pUsbDevice->deviceRequest(forClient, request, &buf, bytesTransferred, kUSBHostStandardRequestCompletionTimeout);
    *state = buf;
    return result;
}

IOReturn Ath3kBT::getVendorVersion(IOService *forClient, struct ath3k_version *version)
{
    StandardUSB::DeviceRequest request;
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeVendor, kRequestRecipientDevice);
    request.bRequest      = ATH3K_GETVERSION;
    request.wValue        = 0;
    request.wIndex        = 0;
    request.wLength       = sizeof(struct ath3k_version);
    uint32_t bytesTransferred = 0;
    IOReturn result = m_pUsbDevice->deviceRequest(forClient, request, version, bytesTransferred, kUSBHostStandardRequestCompletionTimeout);
    return result;
}

bool Ath3kBT::switchPID(IOService *forClient)
{
    StandardUSB::DeviceRequest request;
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeVendor, kRequestRecipientDevice);
    request.bRequest      = USB_REG_SWITCH_VID_PID;
    request.wValue        = 0;
    request.wIndex        = 0;
    request.wLength       = 0;
    uint32_t bytesTransferred = 0;
    IOReturn result = m_pUsbDevice->deviceRequest(forClient, request, (void *)NULL, bytesTransferred, kUSBHostStandardRequestCompletionTimeout);
    return result;
}

bool Ath3kBT::setNormalMode()
{
    unsigned char fw_state;
    bool ret = false;
    StandardUSB::DeviceRequest request;
    if (getVendorState(this, &fw_state) != kIOReturnSuccess) {
        IOLog("%s Can't get state to change to normal mode err\n", getName());
        return ret;
    }
    if ((fw_state & ATH3K_MODE_MASK) == ATH3K_NORMAL_MODE) {
        IOLog("%s firmware was already in normal mode\n", getName());
        return true;
    }
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeVendor, kRequestRecipientDevice);
    request.bRequest      = ATH3K_SET_NORMAL_MODE;
    request.wValue        = 0;
    request.wIndex        = 0;
    request.wLength       = 0;
    uint32_t bytesTransferred = 0;
    IOReturn result = m_pUsbDevice->deviceRequest(this, request, (void *)NULL, bytesTransferred, kUSBHostStandardRequestCompletionTimeout);
    if (result != kIOReturnSuccess) {
        IOLog("%s set normal mode fail\n", getName());
        return false;
    }
    return true;
}

bool Ath3kBT::loadSysCfg()
{
    unsigned char fw_state;
    char filename[ATH3K_NAME_LEN];
    struct ath3k_version fw_version;
    int clk_value;
    bool ret = false;
    
    if (getVendorState(this, &fw_state) != kIOReturnSuccess) {
        IOLog("%s Can't get state to change to load configuration err\n", getName());
        return ret;
    }
    if (getVendorVersion(this, &fw_version) != kIOReturnSuccess) {
        IOLog("Can't get version to change to load ram patch err");
        return ret;
    }
    switch (fw_version.ref_clock) {

    case ATH3K_XTAL_FREQ_26M:
        clk_value = 26;
        break;
    case ATH3K_XTAL_FREQ_40M:
        clk_value = 40;
        break;
    case ATH3K_XTAL_FREQ_19P2:
        clk_value = 19;
        break;
    default:
        clk_value = 0;
        break;
    }
    
    snprintf(filename, ATH3K_NAME_LEN, "ramps_0x%08x_%d%s",
    __le32_to_cpu(fw_version.rom_version), clk_value, ".dfu");
    
    IOLog("%s try to load syscfg file %s\n", getName(), filename);
    
    OSData *fwData = getFWDescByName(filename);
    
    if (fwData == NULL) {
        IOLog("%s can not find syscfg file\n", getName());
        return false;
    }
    
    return loadFwFile(fwData);
}

bool Ath3kBT::loadPatch()
{
    unsigned char fw_state;
    char filename[ATH3K_NAME_LEN];
    struct ath3k_version fw_version;
    __u32 pt_rom_version, pt_build_version;
    bool ret = false;
    
    if (getVendorState(this, &fw_state) != kIOReturnSuccess) {
        IOLog("%s Can't get state to change to load ram patch err\n", getName());
        return ret;
    }
    
    if (fw_state & ATH3K_PATCH_UPDATE) {
        IOLog("%s Patch was already downloaded", getName());
        return true;
    }
    
    if (getVendorVersion(this, &fw_version) != kIOReturnSuccess) {
        IOLog("Can't get version to change to load ram patch err");
        return ret;
    }
    
    snprintf(filename, ATH3K_NAME_LEN, "AthrBT_0x%08x.dfu",
    __le32_to_cpu(fw_version.rom_version));
    
    IOLog("%s try to load patch rom file %s\n", getName(), filename);
    
    OSData *fwData = getFWDescByName(filename);
    
    if (fwData == NULL) {
        IOLog("%s can not find patch rom file\n", getName());
        return false;
    }
    pt_rom_version = get_unaligned_le32((char *)fwData->getBytesNoCopy() +
                        fwData->getLength() - 8);
    pt_build_version = get_unaligned_le32((char *)fwData->getBytesNoCopy() +
                          fwData->getLength() - 4);
    
    if (pt_rom_version != __le32_to_cpu(fw_version.rom_version) ||
        pt_build_version <= __le32_to_cpu(fw_version.build_version)) {
        IOLog("%s Patch file version did not match with firmware\n", getName());
        return false;
    }
    return loadFwFile(fwData);
}

bool Ath3kBT::loadFwFile(OSData *fwData)
{
    int err;
    uint32_t bytesTransferred;
    int count, size;
    
    count = fwData->getLength();
    
    StandardUSB::DeviceRequest ctlreq;
    ctlreq.bmRequestType = USBmakebmRequestType(kRequestDirectionOut, kRequestTypeVendor, kRequestRecipientDevice);
    ctlreq.bRequest = ATH3K_DNLOAD;
    ctlreq.wValue = 0;
    ctlreq.wIndex = 0;
    ctlreq.wLength = FW_HDR_SIZE;
    
    err = m_pUsbDevice->deviceRequest(this, ctlreq, (void *)fwData->getBytesNoCopy(), bytesTransferred, (uint32_t)kUSBHostStandardRequestCompletionTimeout);
    
    char *buf = (char *)fwData->getBytesNoCopy();
    size = fwData->getLength();
    buf += FW_HDR_SIZE;
    size -= FW_HDR_SIZE;
    int ii = 1;
    while (size) {
        int to_send = size < BULK_SIZE ? size : BULK_SIZE;
        
        char buftmp[BULK_SIZE];
        IOMemoryDescriptor * membuf = IOMemoryDescriptor::withAddress(&buftmp, BULK_SIZE, kIODirectionOut);
        if (!membuf) {
            IOLog("%s failed to map memory descriptor\n", getName());
            return false;
        }
        err = membuf->prepare();
        memcpy(buftmp, buf, to_send);
        err = m_pBulkWritePipe->io(membuf, to_send, (IOUSBHostCompletion*)NULL, 0);
        membuf->complete();
        membuf->release();
        if (err) {
            IOLog("%s failed to write firmware to bulk pipe (err:%d, block:%d, to_send:%d)\n", getName(), err, ii, to_send);
            return false;
        }
        buf += to_send;
        size -= to_send;
        ii++;
    }
    return true;
}
