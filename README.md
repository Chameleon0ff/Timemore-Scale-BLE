# TimemoreBLE for ESP32

TimemoreBLE is a C++ library for ESP32 and ESP32-S3 (Arduino framework) that works as a BLE Central and connects directly to Timemore scales.

The library provides a single API for reading weight, controlling tare and timer, and getting battery status across different scale models.

## What This Library Does

1. Scans BLE devices and automatically finds supported Timemore models.
2. Connects to the scale, subscribes to notifications, and initializes the model.
3. Parses weight and battery data and passes it through callbacks.
4. Provides control commands: tare, start/stop/reset timer.
5. Supports auto-reconnect through periodic `update` calls.

## Supported Models

1. Timemore DOT
2. Timemore Basic 3
3. Timemore Classic/Nano

## Technical Foundation

1. Platform: espressif32
2. Framework: Arduino
3. BLE stack: NimBLE-Arduino
4. Protocol:
- For DOT and Basic 3: framed protocol via service FFF0, notify FFF1, command FFF2, CRC16.
- For Classic/Nano: standard Weight Scale service 0x181D.

## Installation

Option 1. Locally in your project:
1. Copy this library into your PlatformIO project's `lib` folder.
2. Add NimBLE-Arduino dependency in `platformio.ini`.

Option 2. As a separate repository via `lib_deps`:
1. Add the library via `lib_deps` in `platformio.ini`.
2. Add NimBLE-Arduino in `lib_deps`.

Minimum dependency:

lib_deps =
    h2zero/NimBLE-Arduino@^2.2.3

See config example in [examples/platformio.ini.example](examples/platformio.ini.example).

## Quick Start

1. Include `TimemoreBLE.h`.
2. Create `TimemoreScaleManager`.
3. Call `connect` in `setup` or in a dedicated FreeRTOS task.
4. Register callbacks for weight and battery.
5. Call `update` regularly in `loop` or your task.

## Key Usage Notes

1. `connect` is a blocking call.
- It performs scan, connection, and model initialization.
- Do not call `connect` from BLE callback context.

2. For real-time workloads, move BLE logic to a separate FreeRTOS task.

3. For inter-task or inter-core weight transfer, use a FreeRTOS queue.

4. Use one manager instance per BLE Central flow.

## Manager API

1. bool connect(uint32_t timeoutMs = 15000)
2. void disconnect()
3. bool isConnected() const
4. TimemoreModel model() const
5. TimemoreScale* scale()
6. void onConnectionChange(std::function<void(bool)> cb)
7. void update()

## Scale API

1. void tare()
2. void setTimer(TimemoreTimerCommand command)
3. float getWeight() const
4. float getWeight2() const
5. uint8_t getBatteryLevel() const
6. void onWeight(std::function<void(float, float)> cb)
7. void onBattery(std::function<void(uint8_t)> cb)

## Library Commands

Weight control commands:
1. tare - reset weight to zero.
2. setTimer START - start timer.
3. setTimer STOP - stop timer.
4. setTimer RESET - reset timer.

Events and state reads:
1. onWeight - receive new weight values.
2. onBattery - receive battery level.
3. getWeight - latest channel 1 weight.
4. getWeight2 - latest channel 2 weight (for Classic/Nano).
5. getBatteryLevel - latest known battery level.

## Project Structure

1. [src/TimemoreProtocol.h](src/TimemoreProtocol.h), [src/TimemoreProtocol.cpp](src/TimemoreProtocol.cpp) - CRC16 and protocol framing.
2. [src/TimemoreScale.h](src/TimemoreScale.h) - base abstract scale interface.
3. [src/TimemoreFramedScale.h](src/TimemoreFramedScale.h) - shared implementation for DOT and Basic 3.
4. [src/TimemoreDotScale.h](src/TimemoreDotScale.h) - DOT model.
5. [src/TimemoreBasicScale.h](src/TimemoreBasicScale.h) - Basic 3 model.
6. [src/TimemoreClassicScale.h](src/TimemoreClassicScale.h) - Classic/Nano model.
7. [src/TimemoreScaleManager.h](src/TimemoreScaleManager.h), [src/TimemoreScaleManager.cpp](src/TimemoreScaleManager.cpp) - scanning, connection, subscriptions, reconnect.
8. [src/TimemoreBLE.h](src/TimemoreBLE.h) - single include entry point for the library.
9. [examples/basic_usage/basic_usage.ino](examples/basic_usage/basic_usage.ino) - integration example.

---

# TimemoreBLE для ESP32

TimemoreBLE - це C++ бібліотека для ESP32 та ESP32-S3 (Arduino framework), яка працює у ролі BLE Central і напряму підключається до ваг Timemore.

Бібліотека надає єдиний API для читання ваги, керування тарою, таймером та отримання стану батареї на різних моделях ваг.

## Що робить ця бібліотека

1. Сканує BLE-пристрої та автоматично знаходить підтримувані моделі Timemore.
2. Підключається до ваг, підписується на нотифікації та ініціалізує модель.
3. Парсить дані ваги та батареї й передає їх через callback.
4. Надає команди керування: tare, start/stop/reset таймера.
5. Підтримує авто-перепідключення через періодичний виклик update.

## Підтримувані моделі

1. Timemore DOT
2. Timemore Basic 3
3. Timemore Classic/Nano

## Технічна основа

1. Платформа: espressif32
2. Фреймворк: Arduino
3. BLE стек: NimBLE-Arduino
4. Протокол:
- Для DOT і Basic 3: фрейми через сервіс FFF0, notify FFF1, command FFF2, CRC16.
- Для Classic/Nano: стандартний Weight Scale сервіс 0x181D.

## Встановлення

Варіант 1. Локально в проєкті:
1. Скопіюйте цю бібліотеку в папку lib вашого PlatformIO-проєкту.
2. Додайте залежність NimBLE-Arduino у platformio.ini.

Варіант 2. Як окремий репозиторій через lib_deps:
1. Підключіть бібліотеку через lib_deps у platformio.ini.
2. Додайте NimBLE-Arduino у lib_deps.

Мінімальна залежність:

lib_deps =
    h2zero/NimBLE-Arduino@^2.2.3

Приклад конфігурації дивіться у [examples/platformio.ini.example](examples/platformio.ini.example).

## Швидкий старт

1. Підключіть заголовок TimemoreBLE.h.
2. Створіть TimemoreScaleManager.
3. Викличте connect у setup або окремій FreeRTOS-задачі.
4. Зареєструйте callback на вагу та батарею.
5. Регулярно викликайте update у loop або задачі.

## Ключові моменти використання

1. connect є блокуючим викликом.
- Метод виконує сканування, підключення і ініціалізацію моделі.
- Не викликайте connect з BLE callback-контексту.

2. Для задач із реальним часом виносьте BLE-логіку в окрему FreeRTOS-задачу.

3. Для міжзадачної або міжядерної передачі ваги використовуйте чергу FreeRTOS.

4. Один екземпляр менеджера на один BLE Central потік.

## API менеджера

1. bool connect(uint32_t timeoutMs = 15000)
2. void disconnect()
3. bool isConnected() const
4. TimemoreModel model() const
5. TimemoreScale* scale()
6. void onConnectionChange(std::function<void(bool)> cb)
7. void update()

## API ваг

1. void tare()
2. void setTimer(TimemoreTimerCommand command)
3. float getWeight() const
4. float getWeight2() const
5. uint8_t getBatteryLevel() const
6. void onWeight(std::function<void(float, float)> cb)
7. void onBattery(std::function<void(uint8_t)> cb)

## Перелік команд бібліотеки

Команди керування вагою:
1. tare - скидання ваги в нуль.
2. setTimer START - запуск таймера.
3. setTimer STOP - зупинка таймера.
4. setTimer RESET - скидання таймера.

Події та читання стану:
1. onWeight - отримання нових значень ваги.
2. onBattery - отримання рівня батареї.
3. getWeight - остання вага каналу 1.
4. getWeight2 - остання вага каналу 2 (для Classic/Nano).
5. getBatteryLevel - останній відомий рівень батареї.

## Структура проєкту

1. [src/TimemoreProtocol.h](src/TimemoreProtocol.h), [src/TimemoreProtocol.cpp](src/TimemoreProtocol.cpp) - CRC16 та фреймінг протоколу.
2. [src/TimemoreScale.h](src/TimemoreScale.h) - базовий абстрактний інтерфейс ваг.
3. [src/TimemoreFramedScale.h](src/TimemoreFramedScale.h) - спільна реалізація для DOT і Basic 3.
4. [src/TimemoreDotScale.h](src/TimemoreDotScale.h) - модель DOT.
5. [src/TimemoreBasicScale.h](src/TimemoreBasicScale.h) - модель Basic 3.
6. [src/TimemoreClassicScale.h](src/TimemoreClassicScale.h) - модель Classic/Nano.
7. [src/TimemoreScaleManager.h](src/TimemoreScaleManager.h), [src/TimemoreScaleManager.cpp](src/TimemoreScaleManager.cpp) - сканування, підключення, підписки, reconnect.
8. [src/TimemoreBLE.h](src/TimemoreBLE.h) - єдина точка підключення бібліотеки.
9. [examples/basic_usage/basic_usage.ino](examples/basic_usage/basic_usage.ino) - приклад інтеграції.