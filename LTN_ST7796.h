#define ST7796_DRIVER

#define SPI_MISO                 12
#define SPI_MOSI                 11
#define SPI_SCLK                 13

// Display control pins
#define TFT_CS                   22
#define TFT_RST                  21
#define TFT_DC                   20
#define TFT_MOSI                 SPI_MOSI
#define TFT_SCLK                 SPI_SCLK
#define TFT_BL                   23
#define TFT_MISO                 SPI_MISO

#define SPI_FREQUENCY            40000000
#define SPI_READ_FREQUENCY       20000000
