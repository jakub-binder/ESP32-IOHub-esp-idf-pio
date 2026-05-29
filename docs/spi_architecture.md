SPI bus je sdílený zdroj.



Na jednom SPI busu může existovat více zařízení.



Příklad:



SPI1

&#x20;├── ADS7961 ADC

&#x20;└── DAC7564 DAC



Každé zařízení má:



\- vlastní CS

\- vlastní SPI mode

\- vlastní clock



Driver zařízení nesmí přímo pracovat

s ESP-IDF SPI API.



Komunikace probíhá přes HAL SPI vrstvu.

