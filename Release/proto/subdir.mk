################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../proto/wlr-layer-shell-unstable-v1-protocol.c \
../proto/xdg-shell-protocol.c 

OBJS += \
./proto/wlr-layer-shell-unstable-v1-protocol.o \
./proto/xdg-shell-protocol.o 

C_DEPS += \
./proto/wlr-layer-shell-unstable-v1-protocol.d \
./proto/xdg-shell-protocol.d 


# Each subdirectory must supply rules for building sources it contributes
proto/%.o: ../proto/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D_GNU_SOURCE -DVERSION='"'"`hg identify`"'"' -I../inc -O3 -Wall -Wextra -c -fmessage-length=0 `pkg-config --cflags gtk+-3.0 gio-unix-2.0` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


