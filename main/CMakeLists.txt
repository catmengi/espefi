# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c vga_disp.c utf8proc.c espefi_api.c espefi_vga.c espefi_event.c espefi_apploader.c espefi_usbhid.c espefi_console.c espefi_shell.c # list the source files of this component
    INCLUDE_DIRS "include"       # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES esp_lcd esp_driver_spi esp_driver_sdspi fatfs espressif__usb_host_hid     # optional, list the public requirements (component names)
    PRIV_REQUIRES      # optional, list the private requirements
)
