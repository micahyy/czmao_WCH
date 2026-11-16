#include "USBconstant.h"
#include "../../config.h"
#include "../report.h"
#include <stdint.h>

/* 与 v1 相同：接口 0 = Boot 键盘，接口 1 = Raw HID（VIA 通道） */
__code USB_Descriptor_Device_t DeviceDescriptor = {
    .Header = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},
    .USBSpecification = VERSION_BCD(1, 1, 0),
    .Class = 0x00, .SubClass = 0x00, .Protocol = 0x00,
    .Endpoint0Size = DEFAULT_ENDP0_SIZE,
    .VendorID = VENDOR_ID, .ProductID = PRODUCT_ID,
    .ReleaseNumber = VERSION_BCD(0, 0, 2),
    .ManufacturerStrIndex = 1, .ProductStrIndex = 2, .SerialNumStrIndex = 3,
    .NumberOfConfigurations = 1
};

__code USB_Descriptor_Configuration_t ConfigurationDescriptor = {
    .Config = {.Header = {.Size = sizeof(USB_Descriptor_Configuration_Header_t), .Type = DTYPE_Configuration},
               .TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t),
               .TotalInterfaces = 2, .ConfigurationNumber = 1, .ConfigurationStrIndex = NO_DESCRIPTOR,
               .ConfigAttributes = (USB_CONFIG_ATTR_RESERVED), .MaxPowerConsumption = USB_CONFIG_POWER_MA(200)},
    .HID_Interface = {.Header = {.Size = sizeof(USB_Descriptor_Interface_t), .Type = DTYPE_Interface},
                      .InterfaceNumber = 0, .AlternateSetting = 0, .TotalEndpoints = 2,
                      .Class = HID_CSCP_HIDClass, .SubClass = HID_CSCP_BootSubclass,
                      .Protocol = HID_CSCP_KeyboardBootProtocol, .InterfaceStrIndex = NO_DESCRIPTOR},
    .HID_KeyboardHID = {.Header = {.Size = sizeof(USB_HID_Descriptor_HID_t), .Type = HID_DTYPE_HID},
                        .HIDSpec = VERSION_BCD(1, 1, 0), .CountryCode = 0x21,
                        .TotalReportDescriptors = 1, .HIDReportType = HID_DTYPE_Report,
                        .HIDReportLength = sizeof(ReportDescriptor)},
    .HID_ReportINEndpoint = {.Header = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},
                             .EndpointAddress = 0x81,
                             .Attributes = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
                             .EndpointSize = 9, .PollingIntervalMS = 1},
    .HID_ReportOUTEndpoint = {.Header = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},
                              .EndpointAddress = 0x01,
                              .Attributes = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
                              .EndpointSize = 9, .PollingIntervalMS = 10},
    .HID_RawInterface = {.Header = {.Size = sizeof(USB_Descriptor_Interface_t), .Type = DTYPE_Interface},
                         .InterfaceNumber = 1, .AlternateSetting = 0, .TotalEndpoints = 2,
                         .Class = HID_CSCP_HIDClass, .SubClass = HID_CSCP_NonBootSubclass,
                         .Protocol = HID_CSCP_NonBootProtocol, .InterfaceStrIndex = NO_DESCRIPTOR},
    .HID_RawDescriptor = {.Header = {.Size = sizeof(USB_HID_Descriptor_HID_t), .Type = HID_DTYPE_HID},
                          .HIDSpec = VERSION_BCD(1, 1, 0), .CountryCode = 0x00,
                          .TotalReportDescriptors = 1, .HIDReportType = HID_DTYPE_Report,
                          .HIDReportLength = sizeof(RawHIDReportDescriptor)},
    .HID_RawReportINEndpoint = {.Header = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},
                                .EndpointAddress = 0x82,
                                .Attributes = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
                                .EndpointSize = EP2_SIZE, .PollingIntervalMS = 20},
    .HID_RawReportOUTEndpoint = {.Header = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},
                                 .EndpointAddress = 0x02,
                                 .Attributes = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
                                 .EndpointSize = EP2_SIZE, .PollingIntervalMS = 20},
};

/* 键盘 + System/Consumer + Mouse + Dial 全部启用（v2 恢复被砍功能） */
__code uint8_t ReportDescriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,
    0x85, REPORT_ID_KEYBOARD,
    0x05, 0x07,
    0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x08, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x05, 0x07, 0x19, 0x00, 0x29, 0xe7, 0x81, 0x00,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x03,
    0xc0,
    /* System Control */
    0x05, 0x01, 0x09, 0x80, 0xa1, 0x01, 0x85, REPORT_ID_SYSTEM,
    0x19, 0x01, 0x2a, 0xb7, 0x00, 0x15, 0x01, 0x26, 0xb7, 0x00,
    0x95, 0x01, 0x75, 0x10, 0x81, 0x00, 0xc0,
    /* Consumer Control */
    0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, REPORT_ID_CONSUMER,
    0x19, 0x01, 0x2a, 0xa0, 0x02, 0x15, 0x01, 0x26, 0xa0, 0x02,
    0x95, 0x01, 0x75, 0x10, 0x81, 0x00, 0xc0,
    /* Mouse */
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01, 0xa1, 0x00,
    0x85, REPORT_ID_MOUSE,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x08, 0x75, 0x01, 0x81, 0x02,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
    0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
    0xc0, 0xc0,
    /* Dial (Surface Dial 风格) */
    0x05, 0x01, 0x09, 0x0e, 0xa1, 0x01, 0x85, REPORT_ID_DIAL,
    0x05, 0x0d, 0x09, 0x21, 0xa1, 0x00,
    0x05, 0x09, 0x09, 0x01, 0x95, 0x01, 0x75, 0x01, 0x15, 0x00, 0x25, 0x01, 0x81, 0x02,
    0x05, 0x01, 0x09, 0x37, 0x95, 0x01, 0x75, 0x0f,
    0x36, 0xf0, 0xf1, 0x46, 0x10, 0x0e, 0x16, 0xf0, 0xf1, 0x26, 0x10, 0x0e, 0x81, 0x06,
    0xc0, 0xc0,
};

__code uint8_t RawHIDReportDescriptor[] = {
    0x06, 0x60, 0xFF,
    0x09, 0x61, 0xa1, 0x01, 0x09, 0x62, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x95, EP2_SIZE, 0x75, 0x08, 0x81, 0x06,
    0x09, 0x63, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, EP2_SIZE, 0x75, 0x08, 0x91, 0x83,
    0xC0
};

__code uint8_t LanguageDescriptor[] = { 0x04, 0x03, 0x09, 0x04 };

__code uint16_t SerialDescriptor[] = {
    (((4 + 1) * 2) | (DTYPE_String << 8)),
    'v','2','0','1',
};
__code USB_Descriptor_String_t ProductDescriptor = {
    .Header = {.Size = sizeof(USB_Descriptor_Header_t) + 11 * sizeof(uint16_t), .Type = DTYPE_String},
    .UnicodeString = {PRODUCT},
};
__code USB_Descriptor_String_t ManufacturerDescriptor = {
    .Header = {.Size = sizeof(USB_Descriptor_Header_t) + 5 * sizeof(uint16_t), .Type = DTYPE_String},
    .UnicodeString = {MANUFACTURER},
};
