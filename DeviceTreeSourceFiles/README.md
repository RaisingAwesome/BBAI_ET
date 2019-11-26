am5729-beagleboneai-custom.dts is a working device tree file that will ensable analog/digital read/write, I2C, and PWM on the BBAI.

To use apply it, do the following steps:

      cd #(or wherever you want to clone the next line to)  
      git clone https://github.com/beagleboard/BeagleBoard-DeviceTrees -b v4.14.x-ti  
      cp am5729-beagleboneai-custom.dts BeagleBoard-DeviceTrees/src/arm/
      cd BeagleBoard-DeviceTrees
      make src/arm/am5729-beagleboneai-custom.dtb  
      sudo cp src/arm/am5729-beagleboneai-custom.dtb /boot/dtbs
      sudo nano /boot/uEnv.txt #(configure: dtb=am5729-beagleboneai-custom.dtb)  
      sudo reboot  
