# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor_bio/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor_bio/include

obj-y   +=  accel.o accel_factory.o

obj-$(CONFIG_MTK_ACCELHUB)		+=	accelhub/
obj-$(CONFIG_MTK_BMI160_SPI)   +=  bmi160-spi/
