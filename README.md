# hpoller
hpoller

Демон для сбора snmp-счетчиков устройств, хранящихся в БД билинга "Гидра" (oracle).

Результаты отправляются плейнтекстом в graphite/carbon-daemon (конфиг, секция "collectors")

---

Требования:

- gcc 4.5.4+
- oracle instant client
- boost
- net-snmp

Установка:

- git clone https://github.com/ircop/hpoller.git
- cd hpoller
- mkdir build && cd build
- cmake ..
- make

- cp ../config.json /etc/hpoller.json
- cp hpoller /usr/bin/hpoller
- hpoller


Конфиг:

- Задайте параметры подключения к своей БД
- "switch_root_id" - ID родительской позиции "Коммутаторы" в номенклатуре
- Остальные параметры должны быть интуитивно понятны :)
