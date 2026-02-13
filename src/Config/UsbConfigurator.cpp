#include "UsbConfigurator.h"

#if defined(DEVICE_M5STICK)
    #include <Services/UsbM5StickService.h>
    #include <Controllers/UsbM5StickController.h>
#else
    // Fallback for all other devices: Cardputer, StampS3, etc.
    #include <Services/UsbS3Service.h>
    #include <Controllers/UsbS3Controller.h>
#endif


UsbComponents UsbConfigurator::configure(ITerminalView& terminalView, IInput& terminalInput, IInput& deviceInput) {
#if defined(DEVICE_M5STICK)
    static ArgTransformer argTransformer;
    static UserInputManager userInputManager(terminalView, terminalInput, argTransformer);
    static UsbM5StickService usbService;
    static UsbM5StickController usbController(terminalView);

    return UsbComponents{
        argTransformer,
        userInputManager,
        usbService,
        usbController
    };
#else
    static ArgTransformer argTransformer;
    static UserInputManager userInputManager(terminalView, terminalInput, argTransformer);
    static UsbS3Service usbService;
    static UsbS3Controller usbController(terminalView, terminalInput, deviceInput, usbService, argTransformer, userInputManager);

    return UsbComponents{
        argTransformer,
        userInputManager,
        usbService,
        usbController
    };
#endif
}
