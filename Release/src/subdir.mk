################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/config.c \
../src/main.c \
../src/map.c \
../src/utils.c \
../src/wofi.c 

OBJS += \
./src/config.o \
./src/main.o \
./src/map.o \
./src/utils.o \
./src/wofi.o 

C_DEPS += \
./src/config.d \
./src/main.d \
./src/map.d \
./src/utils.d \
./src/wofi.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../inc -O3 -Wall -Wextra -c -fmessage-length=0 `pkg-config --cflags gtk+-3.0` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


