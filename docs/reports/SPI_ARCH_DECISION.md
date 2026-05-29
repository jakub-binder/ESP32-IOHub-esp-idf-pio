# Architektonická analýza návrhu nové SPI komponenty

## Účel dokumentu

Tento dokument porovnává tři zadané architektonické varianty nové SPI komponenty v projektu `ESP32-IOHub-esp-idf-pio`.

Hodnocení vychází pouze z:

- existující architektury projektu,
- chování komponenty `i2c_bus`,
- fixture modelu projektu,
- referenčních Arduino souborů:
  - `docs/reference/arduino_legacy/ADC7961-DAC7564/adc_ads7961_spi.cpp`
  - `docs/reference/arduino_legacy/ADC7961-DAC7564/io_ads7961_dac7564_spi.cpp`

Dokument neobsahuje implementaci ani návrh kódu. Cílem je pouze architektonické rozhodnutí mezi variantami A, B a C.

---

# 1. Vstupní architektonický kontext projektu

## 1.1 Co je již v projektu zavedeno

Ze současné architektury projektu vyplývá několik pevných principů:

- board vrstva definuje HW mapování a capabilities,
- fixture vrstva skládá konkrétní systém dohromady,
- runtime (`main.c`) je velmi tenký,
- komponenty mají mít jasné a malé API,
- fixture funguje jako composition root,
- logika HW patří do komponent,
- nemají existovat skryté závislosti,
- nemá vznikat ownership framework ani automatická magie.

To je explicitně potvrzeno jak strukturou projektu, tak README komponenty `i2c_bus`.

---

## 1.2 Co je důležité z komponenty `i2c_bus`

`i2c_bus` je referenční vzor pro nový SPI návrh.

Z jeho dokumentace vyplývá, že:

- drží stav jedné sběrnice v `i2c_bus_t`,
- poskytuje jednoduché API `init/deinit/set_pins_state/is_initialized/port`,
- validuje konfiguraci,
- řeší lifecycle fyzické sběrnice,
- neřeší bus manager,
- neřeší mutexy,
- neřeší refcounting,
- neřeší ownership framework zařízení,
- neřeší registry zařízení,
- fixture zůstává composition root.

To znamená, že referenční vzor není „velký manager“, ale malý infrastrukturní wrapper s explicitním lifecycle.

---

## 1.3 Co je důležité z referenčních Arduino SPI implementací

Z referenčních souborů plyne několik jistých závěrů.

### ADS7961 reference

Soubor `adc_ads7961_spi.cpp` ukazuje, že ADS7961 driver si v Arduino verzi sám řeší:

- SPI begin/end,
- `SPI.beginTransaction(...)` / `SPI.endTransaction()`,
- vlastní `CS` obsluhu,
- vlastní SPI settings,
- vlastní init/deinit,
- vlastní čtení 16bit rámců,
- pipeline čtení přes opakované přenosy.

ADS7961 zde používá `SPI_MODE0`.

### Kombinovaná ADS7961 + DAC7564 reference

Soubor `io_ads7961_dac7564_spi.cpp` ukazuje, že:

- ADC a DAC sdílí jednu SPI sběrnici,
- ADC a DAC mají samostatné CS piny,
- ADC používá `SPI_MODE0`,
- DAC používá `SPI_MODE1`,
- ADC i DAC mají vlastní SPI settings,
- existuje explicitní zajištění, že druhé zařízení není během transakce vybrané,
- řeší se lifecycle sdílené sběrnice,
- řeší se per-device acquire/release logika nad společným bus kontextem.

To znamená, že referenční SPI případ je architektonicky složitější než současný `i2c_bus`, protože SPI zde přirozeně rozlišuje:

- fyzickou sběrnici,
- zařízení na sběrnici.

---

# 2. Hodnoticí kritéria

Každá varianta je hodnocena podle požadovaných hledisek:

- výhody,
- nevýhody,
- složitost,
- konzistence s existující komponentou `i2c_bus`,
- vhodnost pro ADS7961 + DAC7564,
- vhodnost pro budoucí rozšíření projektu.

---

# 3. VARIANTA A

## Definice varianty

Komponenta `spi_bus` pouze inicializuje fyzickou SPI sběrnici.

Device drivery (ADS7961, DAC7564) si interně vytvářejí vlastní `spi_device_handle_t`.

---

## 3.1 Výhody

### Jednoduchá role `spi_bus`
Tato varianta drží `spi_bus` velmi malý, podobně jako `i2c_bus`:

- řeší jen fyzický bus,
- neřeší device abstraction,
- neobsahuje další vrstvu objektů.

### Menší scope infrastrukturní komponenty
Komponenta `spi_bus` by v této variantě zůstala úzká a soustředěná na lifecycle jedné sběrnice.

### Device driver má plnou kontrolu nad svým SPI zařízením
ADS7961 i DAC7564 by si interně nesly vlastní device-level konfiguraci a samy by pracovaly se svým handlem.

To je blízké referenční Arduino implementaci, kde si driver sám nese device-specific SPI settings a device-specific transakční logiku.

---

## 3.2 Nevýhody

### Část SPI ownershipu se přesouvá do device driverů
Tato varianta už není tak čistě konzistentní s aktuální filozofií projektu, kde komponenty neinicializují sdílenou infrastrukturu a fixture je composition root.

I když by `spi_bus` inicializoval fyzickou sběrnici, samotné device drivery by si samy vytvářely své device handly. Tím by část kompoziční odpovědnosti byla ukrytá uvnitř driverů.

### Vzniká skrytá vazba mezi driverem a konkrétním SPI stackem
Driver by nebyl jen driverem zařízení, ale i místem, kde vzniká vazba na ESP-IDF SPI device vrstvu.

To je silnější platformní závislost, než jakou dnes mají I2C komponenty, které dostávají pouze připravený port.

### Fixture ztrácí část kontroly nad skládáním systému
Fixture by sice stále vytvářela instance driverů, ale nevytvářela by plně všechny komunikační objekty. Část wiringu by byla schovaná uvnitř ADS7961 / DAC7564 driveru.

### Horší jednotnost mezi zařízeními
Pokud si každý SPI driver bude interně zakládat device handle po svém, hrozí, že:

- každý driver bude řešit device init trochu jinak,
- každé zařízení bude mít vlastní lifecycle model,
- architektura SPI části bude méně jednotná než u I2C části.

---

## 3.3 Složitost

### Celková složitost
Střední.

`spi_bus` je sice jednoduchý, ale celková složitost systému se přesouvá do device driverů.

To znamená:
- menší složitost infrastrukturní vrstvy,
- větší složitost v ADS7961 a DAC7564 komponentách.

---

## 3.4 Konzistence s existující komponentou `i2c_bus`

Konzistence je **částečná**.

Podobnost s `i2c_bus` je v tom, že bus komponenta řeší jen fyzickou sběrnici.

Rozdíl je v tom, že u I2C device komponenty dostávají již hotový identifikátor sběrnice a samy neprovádějí další infrastrukturu nad bus vrstvou. Ve variantě A by SPI drivery dělaly navíc interní device-level vytvoření handle, tedy další krok, který je mimo čistý pattern současných I2C komponent.

Proto:
- je to podobné na úrovni „bus-only komponenty“,
- ale méně podobné na úrovni composition root a explicitního wiringu.

---

## 3.5 Vhodnost pro ADS7961 + DAC7564

Vhodnost je **dobrá, ale ne ideální**.

Důvod:
- referenční Arduino implementace jasně ukazuje samostatné per-device SPI nastavení,
- ADC a DAC mají rozdílné SPI módy,
- každé zařízení má vlastní CS,
- každé zařízení má vlastní init/deinit.

Varianta A toto umožňuje přirozeně.

Současně ale kombinovaná reference `io_ads7961_dac7564_spi.cpp` ukazuje i bus-level sdílení a koordinaci. Pokud by si každý driver vytvářel device handle sám, bylo by důležité, aby koordinace sdíleného bus lifecycle zůstala stále čitelná a jednotná. To je právě slabší místo této varianty.

---

## 3.6 Vhodnost pro budoucí rozšíření projektu

Vhodnost je **střední**.

Pro menší počet SPI driverů je tato varianta použitelná.

S růstem projektu ale může dojít k tomu, že:
- každý SPI driver bude obsahovat trochu vlastní device binding logiku,
- vzniknou rozdílné device lifecycle patterny,
- bude těžší udržovat jednotný architektonický styl.

Tato varianta je tedy rozšiřitelná, ale méně disciplinovaná než varianta B.

---

# 4. VARIANTA B

## Definice varianty

Komponenta `spi_bus` obsahuje abstrahovanou vrstvu:

- `spi_bus_t`
- `spi_device_t`

A device drivery dostávají již připravený `spi_device_t`.

---

## 4.1 Výhody

### Nejčistší oddělení bus a device vrstvy
Tato varianta přímo odráží to, co ukazuje referenční Arduino implementace:

- existuje jedna fyzická sběrnice,
- na ní existuje více zařízení,
- každé zařízení má vlastní mode,
- každé zařízení má vlastní clock,
- každé zařízení má vlastní CS.

To je přesně model bus + device.

### Fixture zůstává composition root
Tato varianta nejlépe zachovává aktuální filozofii projektu:

- fixture vytváří bus,
- fixture vytváří device objekty,
- fixture předává driverům hotové závislosti,
- device driver si nic skrytě nevytváří.

To je velmi konzistentní s tím, jak dnes fixture vytváří I2C bus a následně zakládá I2C zařízení.

### Device driver se soustředí jen na logiku zařízení
ADS7961 driver by řešil:
- command words,
- pipeline čtení,
- interpretaci RX rámců,
- diagnostiku.

DAC7564 driver by řešil:
- DAC command frame,
- zápisy hodnot,
- device-specific init sekvenci.

Ani jeden z nich by neřešil vytvoření device handle vrstvy. Tím by zůstal jasný a úzký rozsah odpovědnosti.

### Lepší explicitnost architektury
Zvenku by bylo vždy jasné:
- která zařízení existují na sběrnici,
- jak jsou nakonfigurovaná,
- co vytváří fixture,
- co je bus-level a co device-level.

### Jednotnější SPI architektura do budoucna
Pokud časem přibudou další SPI zařízení, budou se připojovat stejným způsobem.

To podporuje konzistenci projektu.

---

## 4.2 Nevýhody

### Vyšší složitost samotné komponenty `spi_bus`
Na rozdíl od `i2c_bus` zde komponenta nebude pouze o jedné struktuře sběrnice. Bude muset pokrývat i device abstraction.

To je větší scope než u `i2c_bus`.

### Méně doslovná podobnost s `i2c_bus`
`i2c_bus` dnes neobsahuje device vrstvu. Varianta B tedy není doslovnou kopií jeho struktury.

Je však důležité rozlišit:
- nekopírovat mechanicky tvar API,
- ale zachovat stejnou filozofii explicitního lifecycle a composition rootu.

Z tohoto pohledu je nevýhodou jen to, že SPI komponenta bude přirozeně dvouúrovňová.

---

## 4.3 Složitost

### Celková složitost
Střední až vyšší.

`spi_bus` bude složitější než `i2c_bus`, protože musí reprezentovat:
- fyzickou sběrnici,
- zařízení na sběrnici.

Na druhou stranu tato složitost odpovídá skutečné struktuře SPI použití v referenčních souborech. Není to uměle přidaná složitost, ale odraz reálného problému.

Současně je tato složitost centralizovaná a architektonicky čistší než rozptýlená složitost ve více device driverech.

---

## 4.4 Konzistence s existující komponentou `i2c_bus`

Konzistence je **vysoká na úrovni filozofie projektu**.

Shodné principy jsou:
- explicitní lifecycle,
- fixture jako composition root,
- žádný bus manager,
- žádný refcount framework jako veřejná architektura,
- žádný registry framework,
- žádná skrytá magie,
- jasné API,
- infrastruktura oddělená od device logiky.

Rozdíl proti `i2c_bus` je pouze v tom, že SPI z povahy věci potřebuje ještě device vrstvu, což je přímo doloženo referenční implementací ADS7961 + DAC7564.

Tato varianta je tedy nejkonzistentnější s architekturou projektu, i když není nejvíce podobná pouze podle počtu struktur.

---

## 4.5 Vhodnost pro ADS7961 + DAC7564

Vhodnost je **nejvyšší ze všech variant**.

Důvod:
- reference jasně ukazuje jednu sdílenou sběrnici,
- reference jasně ukazuje dvě zařízení,
- reference jasně ukazuje rozdílné SPI módy,
- reference jasně ukazuje rozdílné device konfigurace,
- reference jasně ukazuje oddělené CS piny,
- reference fakticky odpovídá modelu bus + device.

Varianta B tento model vystihuje nejpřesněji.

---

## 4.6 Vhodnost pro budoucí rozšíření projektu

Vhodnost je **vysoká**.

Tato varianta poskytuje jednotný vzor pro další SPI zařízení bez toho, aby posouvala odpovědnost za binding do jednotlivých driverů nebo do fixture přes surové ESP-IDF typy.

Z architektonického hlediska jde o nejstabilnější základ pro růst SPI části projektu.

---

# 5. VARIANTA C

## Definice varianty

Komponenta `spi_bus` pouze poskytuje helper funkce nad ESP-IDF SPI API a fixture pracuje přímo s ESP-IDF `spi_device_handle_t`.

---

## 5.1 Výhody

### Minimální wrapper vrstva
Tato varianta přidává nejméně nové abstrakce.

### Plná transparentnost ESP-IDF SPI API
Všechna device-level rozhodnutí jsou přímo vidět ve fixture a nejsou schovaná v driveru.

### Nízká složitost samotné komponenty `spi_bus`
Komponenta by byla velmi malá, případně téměř utilitární.

---

## 5.2 Nevýhody

### Nízká konzistence s projektem jako celkem
Projekt je postaven tak, že komponenty mají mít jasné API a fixture má skládat komponenty, nikoli detailně pracovat s frameworkovými handle typy všude přímo.

U I2C to dnes funguje tak, že fixture pracuje s projektovou komponentou `i2c_bus`, nikoli přímo s nízkoúrovňovým ESP-IDF API v celém rozsahu.

Ve variantě C by se fixture dostala blíž k ESP-IDF SPI detailům než je dnes běžné u zbytku architektury.

### Fixture by nesla příliš mnoho infrastrukturního detailu
Fixture by už nefungovala jen jako composition root, ale i jako místo explicitního sestavování ESP-IDF SPI device handle objektů.

Tím by se část infrastrukturní komplexity přesunula do fixture vrstvy.

### Slabší zapouzdření
Device driver by nedostával projektovou abstrakci, ale přímo ESP-IDF handle.

To zesiluje vazbu vyšších vrstev na konkrétní frameworkový typ.

### Riziko méně čistého oddělení odpovědností
Tato varianta sice nic neschovává, ale také méně chrání architektonické hranice mezi:
- fixture,
- infrastrukturu,
- device driver.

---

## 5.3 Složitost

### Celková složitost
Nízká až střední na úrovni komponenty, ale vyšší na úrovni fixture.

Tedy:
- malá složitost helper vrstvy,
- větší složitost skládání v každé fixture, která SPI použije.

---

## 5.4 Konzistence s existující komponentou `i2c_bus`

Konzistence je **nízká**.

`i2c_bus` je jasná projektová komponenta držící stav sběrnice a poskytující vlastní API. Varianta C by naopak nechávala fixture pracovat přímo s frameworkovým device typem.

To je odlišný styl než u současné I2C části.

---

## 5.5 Vhodnost pro ADS7961 + DAC7564

Vhodnost je **funkčně dostatečná, ale architektonicky slabší**.

ADS7961 + DAC7564 skutečně vyžadují:
- sdílenou sběrnici,
- více zařízení,
- různé SPI módy,
- různé frekvence,
- samostatné CS.

To lze i ve variantě C sestavit.

Nevýhoda ale je, že se tento model nebude vyjadřovat přes projektovou SPI architekturu, ale přes přímou práci fixture s ESP-IDF device handlem. To je méně konzistentní s celkovým stylem projektu.

---

## 5.6 Vhodnost pro budoucí rozšíření projektu

Vhodnost je **nižší než u varianty B**.

S růstem počtu SPI zařízení by se opakovala device-level ESP-IDF skládací logika ve fixture vrstvě. To by zvyšovalo šum a snižovalo jednotnost architektury.

---

# 6. Přímé porovnání variant

## 6.1 Ve vztahu k `i2c_bus`

- **Varianta A**: částečně podobná, ale přesouvá část wiringu do device driverů.
- **Varianta B**: nejvíce zachovává filozofii `i2c_bus`, i když SPI přirozeně rozšiřuje model o device vrstvu.
- **Varianta C**: nejméně konzistentní, protože obchází projektovou abstrahovanou vrstvu a nechává fixture pracovat přímo s ESP-IDF device handlem.

---

## 6.2 Ve vztahu k referenci ADS7961 + DAC7564

- **Varianta A**: umí vyjádřit device-level odlišnosti, ale rozděluje část architektury do driverů.
- **Varianta B**: nejlépe odpovídá reálnému modelu jedné sběrnice a více zařízení.
- **Varianta C**: umí funkčně pokrýt požadavky, ale není to nejčistší architektonické vyjádření.

---

## 6.3 Ve vztahu k budoucímu růstu projektu

- **Varianta A**: použitelná, ale méně jednotná.
- **Varianta B**: nejlépe škálovatelná a nejčitelnější.
- **Varianta C**: s růstem SPI části by zvyšovala detailní infrastrukturní zátěž fixture vrstvy.

---

# 7. Doporučená varianta

## Doporučení

**Doporučená je VARIANTA B.**

---

## Zdůvodnění doporučení

Tato volba nejlépe odpovídá současné architektuře projektu i referenčním SPI zdrojům.

### 1. Nejlépe zachovává fixture jako composition root
Fixture má v tomto projektu skládat systém z připravených komponent a explicitně vytvářet instance závislostí. Varianta B tento princip zachovává nejčistěji.

### 2. Nejlépe odděluje fyzickou sběrnici od zařízení
To přímo odpovídá referenčnímu případu ADS7961 + DAC7564, kde:
- jedna sběrnice je sdílená,
- zařízení jsou dvě,
- každé má vlastní mode,
- každé má vlastní frekvenci,
- každé má vlastní CS.

### 3. Nejlépe chrání device drivery před skrytým infrastrukturním wiringem
ADS7961 a DAC7564 mají řešit protokol zařízení, ne vytváření projektové SPI device vrstvy.

### 4. Je nejvíce konzistentní s filozofií `i2c_bus`
Ne doslovně tvarem, ale principy:
- malá explicitní infrastruktura,
- žádná magie,
- žádný ownership framework,
- fixture drží policy,
- komponenty mají jasné odpovědnosti.

### 5. Je nejvhodnější pro další růst projektu
Pokud přibudou další SPI periferie, varianta B poskytne nejčistší a nejstabilnější architektonický vzor.

---

# 8. Závěrečný shrnující verdikt

Pokud se mají závěry odvodit pouze z:
- stávající architektury projektu,
- referenčního vzoru `i2c_bus`,
- Arduino referencí ADS7961 a ADS7961 + DAC7564,

pak vychází:

- **Varianta A** jako přijatelná, ale méně explicitní v composition rootu,
- **Varianta B** jako architektonicky nejčistší a nejkonzistentnější,
- **Varianta C** jako funkčně možná, ale nejméně sladěná se současnou strukturou projektu.

**Finální doporučení: VARIANTA B.**
