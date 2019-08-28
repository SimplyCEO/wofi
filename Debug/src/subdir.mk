################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/config.c \
../src/main.c \
../src/map.c \
../src/property_label.c \
../src/utils.c \
../src/wofi.c 

OBJS += \
./src/config.o \
./src/main.o \
./src/map.o \
./src/property_label.o \
./src/utils.o \
./src/wofi.o 

C_DEPS += \
./src/config.d \
./src/main.d \
./src/map.d \
./src/property_label.d \
./src/utils.d \
./src/wofi.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D_GNU_SOURCE -I../inc -O0 -g3 -Wall -Wextra -c -fmessage-length=0 -fsanitize=address `pkg-config --cflags gtk+-3.0` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


