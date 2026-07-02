#pragma once
// Stub — ESP32-audioI2S includes this for internet-radio streaming.
// We only use local SD-file playback, so no real WiFi client is needed.
#include <Arduino.h>
#include <IPAddress.h>
#include <Client.h>

class NetworkClient : public Client {
public:
    virtual int     connect(IPAddress, uint16_t)    { return 0; }
    virtual int     connect(const char*, uint16_t)  { return 0; }
    virtual size_t  write(uint8_t)                  { return 0; }
    virtual size_t  write(const uint8_t*, size_t n) { return 0; }
    virtual int     available()                     { return 0; }
    virtual int     read()                          { return -1; }
    virtual int     read(uint8_t*, size_t)          { return 0; }
    virtual int     peek()                          { return -1; }
    virtual void    flush()                         {}
    virtual void    stop()                          {}
    virtual uint8_t connected()                     { return 0; }
    virtual operator bool()                         { return false; }
};
