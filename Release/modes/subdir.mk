################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../modes/dmenu.c \
../modes/drun.c \
../modes/run.c 

OBJS += \
./modes/dmenu.o \
./modes/drun.o \
./modes/run.o 

C_DEPS += \
./modes/dmenu.d \
./modes/drun.d \
./modes/run.d 


# Each subdirectory must supply rules for building sources it contributes
modes/%.o: ../modes/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D_GNU_SOURCE -I../inc -O3 -Wall -Wextra -c -fmessage-length=0 `pkg-config --cflags gtk+-3.0` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


