# STM32_AHT10_MAX7219
Read AHT10 Temperature and Humidity data, then display them on 7-segment LED. 

These *.c files are proved by Nucleo-F411RE board:

AHT10_MAX7219.c: The state machine for initialization AHT10 and MAX7219. 
    Then read temperature and humidity data and display them on 7-segment LED per second.
main.c: check all of codes in "USER CODE BEGIN/END" for add AHT10_MAX7219 state machine process.
AHT10_MAX7219.ioc: The setting of Nucleo-F411RE board; It set I2C1, SPI1 and UART2 port for this project
AHT10_MAX7219.graphml: state machine diagram in yEd. It should be opened by yEd graph editor. yEd web link: https://www.yworks.com/products/yed
