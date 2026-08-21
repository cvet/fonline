---
layout: default
title: Сеть и авторитетность
locale: ru
document_id: networking
permalink: /Docs/ru/explanation/authority-and-networking/
---

# Сеть и авторитетность

<!-- docs-translation: {"document_id":"networking","locale":"ru","source_path":"Docs/en/explanation/authority-and-networking/index.md","source_sha256":"cb60b35c27cd62f905be7eaa5e030e3a73b08ead3e3d342d40e00da3b25267ca"} -->

Этот документ описывает переиспользуемые сетевые слои движка: буферы сообщений, обработку отладочных hash, клиентские и серверные абстракции соединений и упорядоченный UDP-транспорт.

Используйте его при изменении `Source/Common/NetBuffer.*`, `NetworkUdp.*`, `Source/Client/NetworkClient*`, `Source/Server/NetworkServer*` или сетевых тестов.

## Модель владения

Движок владеет абстракциями транспорта, framing сообщений, поведением упорядоченного UDP и интерфейсами клиентских и серверных соединений. Подключающий проект владеет топологией развёртывания, публичными адресами серверов, эксплуатационной политикой и проектным использованием команд.

Не документируйте здесь хосты, порты или инфраструктуру релиза конкретного проекта.

## Проверенные пути исходного кода

- `Source/Common/NetBuffer.h`
- `Source/Common/NetBuffer.cpp`
- `Source/Common/NetworkUdp.h`
- `Source/Common/NetworkUdp.cpp`
- `Source/Common/Settings.inc`
- `Source/Client/NetworkClient.h`
- `Source/Client/NetworkClient-Interthread.cpp`
- `Source/Client/NetworkClient-Sockets.cpp`
- `Source/Client/NetworkClient-UdpSockets.cpp`
- `Source/Server/NetworkServer.h`
- `Source/Server/NetworkServer-Interthread.cpp`
- `Source/Server/NetworkServer-UdpSockets.cpp`
- `Source/Server/NetworkServer-Asio.cpp`
- `Source/Server/NetworkServer-WebSockets.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/ServerConnection.h`
- `Source/Server/ServerConnection.cpp`
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`

## Буферы сообщений

`Source/Common/NetBuffer.h` определяет общий бинарный слой сообщений:

- `NetBuffer` — общее хранилище, рост, состояние ключа шифрования и сырое копирование;
- `NetOutBuffer` — запись и framing исходящих сообщений;
- `NetInBuffer` — чтение и framing входящих сообщений.

Важные константы:

- `CRYPT_KEYS_COUNT = 50`
- `NETMSG_SIGNATURE = 0x011E9422`

Обязанности `NetOutBuffer`:

- добавление сырых байтов через `Push()`;
- запись арифметических типов, enum и простых типов свойств через типизированный `Write()`;
- запись строк и хешированных строк;
- запись блоков данных свойств через `WritePropsData()`;
- framing сообщений через `StartMsg()` и `EndMsg()`.

Обязанности `NetInBuffer`:

- накопление входящих байтов через `AddData()`;
- определение готовности полного сообщения через `NeedProcess()`;
- чтение типизированных значений, строк и хешированных строк;
- чтение данных свойств через `ReadPropsData()`;
- разбор идентификаторов сообщений через `ReadMsg()`;
- сжатие и сброс прочитанных буферов после обработки.

Синхронизация свойств и передача состояния сущностей должны использовать эти помощники, а не самодельные бинарные раскладки.

<a id="inbound-hardening-untrusted-client-server"></a>

## Защита входящего потока (недоверенный клиент → сервер)

Сервер считает все входящие байты враждебными. Несколько дополняющих друг друга слоёв защищают от исчерпания ресурсов и неверных данных:

- **Проверка длины до выделения памяти.** Любая объявленная peer длина или количество проверяется относительно реально оставшихся в буфере байтов *до* выделения или цикла. `NetInBuffer::Read<string>()` и `NetInBuffer::ReadPropsData()` выбрасывают `NetBufferException`, если длина больше `GetUnreadSize()`, поэтому маленькое сообщение не может спровоцировать многогигабайтное выделение. Декодирование входящего remote call использует read-only `DataReader`: байты строки сначала проверяются как заимствованный view, затем создаётся владеющая строка; количества массивов, словарей и словарей массивов сопоставляются с оставшимся payload по минимальному wire-размеру типа до `Reserve()`, создания контейнера или цикла по count. Серверный валидатор содержимого выполняет такую же предварительную проверку. Префиксы, которые физически не помещаются в payload, отклоняются без отдельного фиксированного лимита для корректных вызовов, поэтому compatibility version не меняется.
- **Максимальный размер сообщения.** `NetInBuffer::SetMaxMsgLen(len)` задаёт верхнюю границу одного framed-сообщения. `NeedProcess()` выбрасывает `UnknownMessageException`, приводящий к жёсткому отключению, уже при чтении заголовка, если `msg_len` превышает предел, не накапливая payload в receive-буфере. Сервер устанавливает значение из `ServerNetwork.MaxMessageSize`, где `0` означает отсутствие ограничения. Клиент лимит не задаёт, чтобы большие синхронизации сервер-клиент продолжали работать. Все входящие серверу сообщения являются небольшими управляющими сообщениями, поэтому значение по умолчанию заметно выше корректной нагрузки.
- **Бюджет сообщений одного прохода.** Сервер извлекает не более `ServerNetwork.MaxMessagesPerProcessPass` сообщений одного соединения за один проход worker-задания и затем уступает выполнение. Периодическое задание игрока планируется снова, а остаток буфера обрабатывается следующим проходом; одно флудящее соединение не монополизирует поток, общий с заданиями мира.
- **Окно переупорядочивания UDP.** `UdpTransportOptions.MaxReorderAhead`, на сервере задаваемый `ServerNetwork.MaxUdpReorderAhead`, ограничивает, насколько далеко вперёд от ожидаемого sequence карта переупорядочивания `_receivedPackets` хранит пакеты. Пакеты вне окна отбрасываются и повторно отправляются отправителем, поэтому peer, не присылающий следующий пакет по порядку, не может неограниченно увеличивать карту.
- **Ошибки разбора до рукопожатия остаются эксплуатационным шумом.** Неверный payload, вызвавший `NetBufferException` до `ServerConnection::IsHandshakeComplete()`, один раз записывается как предупреждение invalid-handshake с remote host/port и приводит к жёсткому отключению без глобального exception reporter. После завершённого рукопожатия то же исключение следует обычному пути отчёта. Различие закрепляет `ServerRejectsMalformedPreHandshakePayloadWithoutExceptionReport` из `Source/Tests/Test_ClientServerIntegration.cpp`.

Посимвольный валидатор содержимого `ClientDataValidation.*`, вызываемый для клиентских записей свойств и входящих remote-call payload, дополняет эти слои. Он требует конечные float, корректный UTF-8, отсутствие встроенного NUL в строках — NUL допустим в UTF-8, но неприемлем в клиентском тексте и опасен для C-строк, журналов и БД, — неотрицательные размеры, согласованность count и payload и разрешаемые enum, hash и proto. Он не устанавливает фиксированный максимум длины строки или числа элементов; абсолютные пределы длины и флуда принадлежат верхнему слою буфера и транспорта.

## Hash

Сетевые буферы сериализуют `hstring`: `NetOutBuffer` пишет 64-битный hash, а `NetInBuffer` через `HashResolver` разрешает его обратно в строку.

При изменении сериализации hash проверяйте и сгенерированные метаданные и регистрацию hash, и сетевых потребителей runtime.

### Восстановление неизвестного hash

Клиент и сервер независимо строят hash-хранилища из локальных ресурсов, поэтому сервер может передать созданный в runtime `hstring` или строку из отсутствующего у клиента содержимого. `NetInBuffer::ReadHashedString` разрешает сырой hash через переданный `HashResolver`. При неудаче обработчик видит сырой `hstring::hash_t`, входной буфер сбрасывается, а `ReadHashedString` выбрасывает обычный `NetBufferException`. Тот же обработчик покрывает ленивое разрешение вне буфера, например преобразование сырых реплицированных свойств в AngelScript `hstring`, массив, словарь или объект proto-ссылки.

Движок восстанавливается вместо бесконечного цикла отключений:

1. `ClientEngine` регистрирует обработчик ошибки разрешения `HashStorage`. Когда на установленном соединении клиент встречает неизвестный hash, обработчик пишет `NetMessage::UnresolvedHash` и один раз немедленно сбрасывает ожидающий вывод. Для прямого чтения буфера следующий `NetBufferException` всё равно приводит `ClientConnection::Process` к обычному отключению; ленивое исключение скрипта или свойства может быть удержано системой событий, поэтому сервер после получения hash также жёстко отключает отправителя. Сообщение мало, соединение только что было живым, и оно попадает в send-буфер ядра без ожидания или retry busy-loop. Если зависший socket его потеряет, клиент повторно сообщит тот же hash при следующем столкновении. Клиент не хранит состояние, ничего не пишет на диск и получает строку при следующем обычном переподключении.
2. Серверный `Process_UnresolvedHash` разрешает заявленный hash в собственном хранилище, записывает его в журнал и, если строка известна, сохраняет её в постоянной коллекции `HashReports`, где ключом является строка, и в памяти. Неизвестные и серверу hash один раз за сеанс записываются, но не сохраняются. Если транспорт сообщает закрытие раньше, чем worker достигнет уже доставленного ввода, перед очисткой сервер проверяет жёстко отключённое соединение на ожидающий `UnresolvedHash`. Затем сервер выполняет `HardDisconnect`, поскольку сообщивший неверный hash клиент уже прекратил разбор stream и переподключается; это также покрывает клиента, который сообщил hash, но не отключился сам.
3. Сервер рассылает новую известную строку всем уже подключённым клиентам как `NetMessage::HashList`, а при каждом рукопожатии отправляет полный накопленный набор сразу после `InitData` через `SendAllReportedHashes`. `HashList` состоит из count и length-prefixed строк.
4. Клиенты передают каждую полученную строку в `HashResolver::ToHashedString`, регистрируя тот же hash локально. Последующие разрешения проходят. Поскольку полный набор отправляется на каждом подключении, клиент, который сообщил hash и отключился, получает его после reconnect.

Заявленные строки хранятся сырыми, не регистрируясь в серверном hash storage, чтобы сервер мог сохранять и пересылать их без воссоздания мёртвых записей. При запуске сервер загружает коллекцию `HashReports` после статического содержимого, но до создания runtime-строк и мира, и проверяет каждую строку через не добавляющий записи `HashStorage::CheckHashedString`. Если строка разрешается, пробел считается устранённым добавлением содержимого и запись удаляется из хранилища и рассылки. Всё ещё неизвестная строка записывается с предупреждением, сохраняется и рассылается, поскольку исходное содержимое по-прежнему отсутствует.

Это изменение сериализованного контракта: добавлены `NetMessage::HashList` от сервера к клиенту и `NetMessage::UnresolvedHash` от клиента к серверу, поэтому центральный compatibility marker в `Source/Common/Common.h` увеличен соответствующим образом.

## Абстракция клиентского соединения

`Source/Client/NetworkClient.h` определяет `NetworkClientConnection`.

Сжатый трафик клиента и сервера является единым непрерывным zlib-stream, сбрасываемым через `Z_SYNC_FLUSH`; транспортное чтение может делить или объединять его байты и не представляет независимые сжатые пакеты. Неверный ввод невозможно пропустить или повторно синхронизировать внутри того же соединения. `StreamDecompressor` сообщает ошибку peer-stream как `DecompressException`, а `ClientConnection` считает её ошибкой протокола: записывает, отключается и сбрасывает буферы и декомпрессор, чтобы reconnect начал чистый stream. Он не повторяет те же байты, не продолжает испорченный stream и не считает ошибку декомпрессии условием fallback с UDP на TCP.

Открытая поверхность не зависит от транспорта:

- `IsConnecting()` / `IsConnected()`;
- счётчики байтов `GetBytesSend()`, `GetBytesReceived()`;
- `CheckStatus(for_write)`;
- `SendData()`;
- `ReceiveData()`;
- `Disconnect()`.

Фабрики выбирают реализацию транспорта:

- `CreateInterthreadConnection()`
- `CreateSocketsConnection()`
- `CreateUdpSocketsConnection()`

Конкретные файлы:

- `NetworkClient-Interthread.cpp`
- `NetworkClient-Sockets.cpp`
- `NetworkClient-UdpSockets.cpp`

Клиентская среда выполнения по возможности должна зависеть от абстрактного интерфейса соединения; особенности транспорта принадлежат файлам реализации.

## Абстракция серверного соединения

`Source/Server/NetworkServer.h` определяет две серверные абстракции:

- `NetworkServerConnection` — одно принятое или активное соединение;
- `NetworkServer` — жизненный цикл слушающего сервера.

`NetworkServerConnection` владеет регистрацией и диспетчеризацией callback:

- `SetAsyncCallbacks(send, receive, disconnect)`;
- `Dispatch()`;
- `Disconnect()`;
- `GetHost()` / `GetPort()`;
- `IsDisconnected()`.

Send callback возвращает outgoing bytes **по значению**, и каждый transport владеет buffer, переданным socket. Borrowed sender buffer может быть перезаполнен другим dispatch или освобождён при disconnect, пока I/O thread ещё выполняет compression/send. `Disconnect()` очищает callback под тем же lock, который защищает invocation, поэтому teardown ждёт in-flight pull, а последующие transport ticks не достигают исчезающего sender.

`NetworkServer` хранит слабые ссылки на каждое принятое соединение. `Shutdown()` сначала закрывает регистрацию относительно конкурентных accept, делает снимок всех ещё живых соединений и отключает их, затем выполняет зависящую от транспорта остановку listener/io-context и присоединение потока. Соединение, принятое одновременно с остановкой, либо входит в снимок, либо отклоняется и отключается `TrackConnection()`; оно не может потеряться между accept callback и завершением io-thread. Повторный `Shutdown()` ничего не делает.

К неавторизованным соединениям сервер применяет два независимых ограничения:

- `ServerNetwork.InactivityDisconnectTime` ограничивает тишину между любыми входящими сообщениями;
- `ServerNetwork.LoginTimeout` ограничивает время без значимого прогресса до входа, а `0` отключает лимит. Рукопожатие, authentication remote call и запросы update-файлов обновляют прогресс; транспортные ping не обновляют. Законный updater может продолжать работу, но peer не удерживает неавторизованный slot одними ответами ping.

Авторизованное соединение также отключается, если перестаёт отвечать на ping. `ServerNetwork.ClientPingTime` задаёт interval; если предыдущий ping остаётся без ответа к моменту следующего, сервер записывает `PingTimeout` и выполняет hard disconnect.

### Причины отключения

Каждое закрытие записывает причину в `DisconnectReason`, а `HardDisconnect(reason)` требует от caller выбрать её:

| Причина | Основание |
|---|---|
| `None` | соединение ещё активно |
| `ClientClosed` | transport сообщил об исчезновении peer; добровольный выход и потеря сети здесь неразличимы |
| `InactivityTimeout` | до `InactivityDisconnectTime` не пришло входящее сообщение |
| `PingTimeout` | предыдущий ping остался без ответа |
| `LoginTimeout` | до `LoginTimeout` не было pre-login progress |
| `ProtocolError` | malformed/unexpected data или failed connection publication |
| `UpdaterError` | неверный запрос update file |
| `ServerShutdown` | штатная остановка сервера |
| `ScriptRequest` | вызов `Player.HardDisconnect()` из script |
| `LoginFailed` | login откатился после server-side failure |
| `ReplacedByReconnect` | новый login того же account заменил session |

Побеждает первая записанная причина: поздний generic callback `ClientClosed` от transport не перезаписывает конкретное основание. Причина входит в log закрытого соединения и доступна handlers `OnPlayerLogout` через `Player.GetDisconnectReason()`. Контракт закреплён `ServerConnectionRecordsWhyItWasDisconnected`.

`ServerDisconnectsPreLoginConnectionAfterLoginTimeout` проверяет runtime deadline, а `NetworkServerInterthreadCopiedListenerRejectsAfterShutdown` и тесты остановки транспортов — владение принятыми соединениями и отказ конкурентному accept.

`NetworkServer` запускает реализации через фабрики:

- `StartInterthreadServer()`;
- `StartUdpSocketsServer()`;
- `StartAsioServer()` при `FO_HAVE_ASIO`;
- `StartWebSocketsServer()` при `FO_HAVE_WEB_SOCKETS`;
- `CreateDummyConnection()` для тестов и специальных путей.

Порты прослушивания и endpoint подключения клиента настраиваются отдельно для каждого транспорта:

- **TCP** слушает `Network.ServerPort`, **UDP** — `Network.ServerPort + Network.UdpPortOffset`;
- **WebSocket(S)** слушает `Network.WebSocketPort`;
- клиент подключает plain TCP/UDP к `ClientNetwork.ServerHost`:`Network.ServerPort`, а WebSocket(S) — к `ClientNetwork.WebSocketHost`:`Network.WebSocketPort`. WebSocket endpoint сохраняет hostname для TLS-сертификата, а TCP/UDP endpoint может быть сырым IP, позволяя native-клиенту не выполнять DNS.

Каждый endpoint задаётся явно: WebSocket host и port независимы и не выводятся из `ServerHost` / `ServerPort`.

Диагностика socket не должна напрямую использовать `std::error_code::message()`, `std::system_error::what()`, `strerror()` или `FormatMessage()`, поскольку они возвращают текст в локали ОС. Направляйте ошибки через `net_sockets::error_text()`: он сопоставляет частые сетевые состояния стабильным английским именам, всегда сохраняет native category и числовой code, а неизвестное состояние возвращает как `Network error (<category>:<code>)`. Исключение запуска listener также должно добавлять транспорт и порт, чтобы занятый TCP или WebSocket endpoint диагностировался без локализованного системного текста.

Конкретные файлы:

- `NetworkServer-Interthread.cpp`
- `NetworkServer-UdpSockets.cpp`
- `NetworkServer-Asio.cpp`
- `NetworkServer-WebSockets.cpp`

### Lifetime и потоки асинхронного транспорта

Socket-соединения сервера Asio и WebSockets выполняют io-loop в выделенном потоке, а worker pool движка вызывает `Dispatch()`/`Disconnect()` соединения из других потоков. Lifetime обёртки должен соблюдать следующие правила:

- **Callback io-thread не должен разыменовывать обёртку после того, как движок её отпустил.** Asio обеспечивает это захватом `shared_from_this()` каждым async read/write handler. WebSockets подключает постоянные websocketpp handler после конструктора через `Start()`, поскольку `weak_from_this()` неприменим в конструкторе, а каждый handler блокирует `weak_from_this()` до обращения к обёртке. Захват сырого `this` создаёт use-after-free.
- **Межпоточное завершение должно использовать thread-safe путь транспорта.** Для WebSockets это `connection->close()`, ставящий работу в io service, а не доступный только io-thread `connection->terminate()`.
- **Обёртка не должна продлевать lifetime нижележащего соединения за lifetime владеющего io_context.** Websocketpp endpoint владеет соединением и его привязанными к io_context Asio-таймерами и уничтожает его в io-thread. Поэтому обёртка хранит слабую ссылку и блокирует её на каждый вызов. Сильная ссылка позволила бы выжившей обёртке уничтожить соединение уже после io_context, создавая use-after-free при остановке.

`Test_NetworkServer.cpp` проверяет каждый транспорт end-to-end: interthread, повторный accept Asio и остановку с принятым TCP-соединением, а также настоящий websocketpp-клиент, отправляющий frame и полагающийся на остановку сервера для отключения. Запускайте тест под AddressSanitizer, чтобы защищать эти lifetime-правила.

## Упорядоченный UDP-транспорт

`Source/Common/NetworkUdp.h` реализует упорядоченный и надёжный слой payload поверх UDP.

Типы пакетов:

- `Connect`
- `Accept`
- `Payload`
- `KeepAlive`
- `Disconnect`

`UdpTransportOptions` управляет:

- `MaxPayload`
- `MaxPendingBytes`
- `MaxReorderAhead`
- `ResendTimeoutMs`
- `ConnectRetryMs`
- `Redundancy`

`UdpPacketInfo` содержит разобранные данные пакета:

- тип пакета;
- session ID;
- sequence number;
- acknowledgement sequence;
- acknowledgement bitmask;
- дополнительное значение;
- байты payload.

`UdpOrderedChannel` владеет состоянием сеанса и надёжным порядком:

- состояние сеанса: `GetSessionId()`, `HasSession()`, `SetSessionId()`, `Reset()`;
- готовность вывода: `NeedSend()`, `CanAcceptPayload()`;
- создание и повторная отправка пакета: `PrepareOutput()`;
- обработка входящего sequence: `HandleIncomingPayload()`;
- упорядоченная доставка: `HasReadyData()`, `ExtractReadyData()`;
- отключение: `MakeDisconnectPacket()`.

Самостоятельные помощники:

- `MakeUdpConnectPacket()`
- `MakeUdpAcceptPacket()`
- `TryParseUdpPacket()`

При изменении UDP проверяйте acknowledgement, ограничения ожидающих байтов, окно переупорядочивания, время resend, разбор пакета, отключение и избыточные хвостовые пакеты.

## Связь с состоянием сущностей и свойств

Синхронизация сущностей и свойств использует метаданные свойства, чтобы решить, что можно отправить, и сетевые буферы для сериализации.

Значимые флаги из [модели сущностей](../entity-and-property-model/):

- `Synced`
- `OwnerSync`
- `PublicSync`
- `NoSync`
- `ModifiableByClient`
- `ModifiableByAnyClient`

Сетевое изменение, затрагивающее репликацию свойства, должно рассматриваться вместе с документацией и тестами сущностей и свойств.

## Выбор транспорта

Исходное дерево поддерживает несколько семейств соединений:

- interthread для связи внутри процесса и тестов;
- socket-транспорты;
- UDP socket на основе `UdpOrderedChannel`;
- сервер ASIO при сборке с `FO_HAVE_ASIO`;
- сервер WebSocket при сборке с `FO_HAVE_WEB_SOCKETS`.

Доступность определяется compile-time feature toggles и зависимостями платформы. Сборочные переключатели и package workflow описаны в [процессе сборки](../../how-to/build/) и [конвейере BuildTools](../../reference/cmake-and-buildtools/pipeline.md).

## Тесты для проверки

Подходящие тесты:

- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp` — внутрипроцессное рукопожатие клиента и сервера и путь события соединения.

## Маршрутизация изменений

- Бинарный framing, шифрование и сериализация hash: `Source/Common/NetBuffer.*`.
- Упорядоченный UDP: `Source/Common/NetworkUdp.*`.
- Клиентская абстракция транспорта и реализации: `Source/Client/NetworkClient*`.
- Серверная абстракция транспорта и реализации: `Source/Server/NetworkServer*`.
- Семантика репликации сущностей и свойств: [модель сущностей](../entity-and-property-model/) и сгенерированный код метаданных и свойств.
- Переключатели возможностей сборки: [процесс сборки](../../how-to/build/) и [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md).

## Контрольный список проверки

1. Запустите относящиеся к изменённому транспорту тесты UDP, клиента и сервера.
2. Проверьте пути connect/accept и disconnect.
3. При изменении `NetInBuffer` / `NetOutBuffer` проверьте частичные чтения и framing сообщений.
4. Проверьте разрешение hash и поведение debug hash между клиентской и серверной сборками.
5. При изменении раскладки сообщения или сериализации property data проверьте синхронизацию свойств.
6. При изменении ASIO, WebSocket или socket-кода проверьте доступность транспорта на каждой платформе.
