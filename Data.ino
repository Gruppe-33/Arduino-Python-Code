

class ILedController
{
public:
    virtual void SetR(uint8_t amount)
    {
        m_RColor = amount;
    }
    virtual void SetG(uint8_t amount)
    {
        m_GColor = amount;
    }
    virtual void SetB(uint8_t amount)
    {
        m_BColor = amount;
    }

    virtual void GetCurrenRGB(uint8_t Rgb[3])
    {
        Rgb[0] = m_RColor;
        Rgb[1] = m_GColor;
        Rgb[2] = m_BColor;
    }

    uint8_t m_RColor;
    uint8_t m_GColor;
    uint8_t m_BColor;

    void SetHSV(uint16_t h, float s, float v)
    {
        double C = s * v;
        double X = C * (1 - fabs(fmod(static_cast<float>(h) / 60.0, 2) - 1));
        double m = v - C;
        double Rs, Gs, Bs;

        if (h >= 0 && h < 60)
        {
            Rs = C;
            Gs = X;
            Bs = 0;
        }
        else if (h >= 60 && h < 120)
        {
            Rs = X;
            Gs = C;
            Bs = 0;
        }
        else if (h >= 120 && h < 180)
        {
            Rs = 0;
            Gs = C;
            Bs = X;
        }
        else if (h >= 180 && h < 240)
        {
            Rs = 0;
            Gs = X;
            Bs = C;
        }
        else if (h >= 240 && h < 300)
        {
            Rs = X;
            Gs = 0;
            Bs = C;
        }
        else
        {
            Rs = C;
            Gs = 0;
            Bs = X;
        }

        this->SetR((Rs + m) * 255);
        this->SetG((Gs + m) * 255);
        this->SetB((Bs + m) * 255);
    }
};

class RGBLedController
    : public ILedController
{
public:
    RGBLedController(uint8_t rPin,
                     uint8_t gPin,
                     uint8_t bPin)
        : m_RPin(rPin), m_GPin(gPin), m_BPin(bPin)
    {
        pinMode(rPin, OUTPUT);
        pinMode(gPin, OUTPUT);
        pinMode(bPin, OUTPUT);
    }

    uint8_t m_RPin;
    uint8_t m_GPin;
    uint8_t m_BPin;

    virtual void SetR(uint8_t amount) override
    {
        ILedController::SetR(amount);

        analogWrite(m_RPin, m_RColor);
    }

    virtual void SetG(uint8_t amount) override
    {
        ILedController::SetG(amount);

        analogWrite(m_GPin, m_GColor);
    }

    virtual void SetB(uint8_t amount) override
    {
        ILedController::SetB(amount);

        analogWrite(m_BPin, m_BColor);
    }
};

class LightReader
{
public:
    LightReader(uint8_t outputPin,
                uint8_t inputPin)
        : m_OutputPin(outputPin), m_InputPin(inputPin)
    {
        pinMode(m_OutputPin, OUTPUT);
        pinMode(m_InputPin, INPUT);
    }

    bool SetState(bool enabled)
    {
        digitalWrite(this->m_OutputPin, enabled ? HIGH : LOW);
    }

    uint16_t ReadValue()
    {
        return analogRead(this->m_InputPin);
    }

    uint8_t m_OutputPin;
    uint8_t m_InputPin;
};

static uint16_t g_LightDelay = 150;

#define SERIN_MAGIC 0x89af

#define SERIN_SWEEPRGB 0x1
#define SERIN_SWEEP 0x2
#define SERIN_CUSTOM 0x3
#define SERIN_SET_DELAY 0x4
#define SERIN_GET_DELAY 0x5

#pragma push(pack, 1)
struct SerinHeader
{
public:
    uint16_t m_Magic;      //0x0000
    uint16_t m_Id;         //0x0002
    uint16_t m_Command;    //0x0004
    uint16_t m_DataLength; //0x0006
};

#pragma pop(pack)

// Globals
static ILedController *g_pLed = nullptr;
static LightReader *g_pLight = nullptr;

void SweepSpectrum(uint16_t id, void *pData, uint32_t dataSize)
{
    for (uint16_t i = 0; i < 360; i++)
    {
        g_pLed->SetHSV(i, 1.0f, 1.0f);

        delay(g_LightDelay);

        auto Value = g_pLight->ReadValue();

        Serial.print("D:1:");
        Serial.print(id);
        Serial.print(":");
        Serial.print(i);
        Serial.print(",");
        Serial.println(Value);
    }

    g_pLed->SetR(0);
    g_pLed->SetG(0);
    g_pLed->SetB(0);
}

void SweepLight(uint16_t id, void *pData, uint32_t dataSize)
{
    for (uint16_t i = 0; i < 255; i++)
    {
        g_pLed->SetR(i);
        g_pLed->SetG(i);
        g_pLed->SetB(i);

        delay(g_LightDelay);

        auto Value = g_pLight->ReadValue();

        Serial.print("D:2:");
        Serial.print(id);
        Serial.print(":");
        Serial.print(i);
        Serial.print(",");
        Serial.println(Value);
    }

    g_pLed->SetR(0);
    g_pLed->SetG(0);
    g_pLed->SetB(0);
}

void CustomLight(uint16_t id, void *pData, uint32_t dataSize)
{
#pragma push(pack, 1)
    struct CustomLight
    {
        uint8_t m_R;
        uint8_t m_G;
        uint8_t m_B;
    };
#pragma pop(pack)

    if (pData == nullptr)
    {
        Serial.println("E:InvalidInput");
        return;
    }

    if (dataSize < sizeof(CustomLight))
    {
        Serial.println("E:InvalidInput");
        return;
    }

    CustomLight *pLight = reinterpret_cast<CustomLight *>(pData);

    g_pLed->SetR(pLight->m_R);
    g_pLed->SetG(pLight->m_G);
    g_pLed->SetB(pLight->m_B);

    delay(g_LightDelay);

    auto Value = g_pLight->ReadValue();

    Serial.print("D:3:");
    Serial.print(id);
    Serial.print(":");
    Serial.print(pLight->m_R);
    Serial.print(",");
    Serial.print(pLight->m_G);
    Serial.print(",");
    Serial.print(pLight->m_B);

    Serial.print(",");
    Serial.println(Value);

    g_pLed->SetR(0);
    g_pLed->SetG(0);
    g_pLed->SetB(0);
}

void setup()
{
    Serial.begin(9600);

    g_pLed = new RGBLedController(3, 5, 6);

    g_pLight = new LightReader(0, A0);
}

void loop()
{
    if (Serial.available() == 0)
    {
        delay(500);
        return;
    }

    auto DataSize = Serial.available();

    if (DataSize < sizeof(SerinHeader))
    {
        Serial.println("E:Bad input!");
        return;
    }

    SerinHeader Header;

    Serial.readBytes(reinterpret_cast<uint8_t *>(&Header), sizeof(SerinHeader));

    if (Header.m_Magic != SERIN_MAGIC)
    {
        Serial.print(Header.m_Magic);
        Serial.println("E:Bad magic!");
        return;
    }

    uint8_t *pData = nullptr;

    if (Header.m_DataLength > 0)
    {
        pData = new uint8_t[Header.m_DataLength];

        while (Serial.available() < Header.m_DataLength)
            delay(50);

        Serial.readBytes(pData, Header.m_DataLength);
    }

    switch (Header.m_Command)
    {
    case SERIN_SWEEPRGB:
        SweepSpectrum(Header.m_Id, pData, Header.m_DataLength);
        break;
    case SERIN_SWEEP:
        SweepLight(Header.m_Id, pData, Header.m_DataLength);
        break;
    case SERIN_CUSTOM:
        CustomLight(Header.m_Id, pData, Header.m_DataLength);
        break;
    case SERIN_SET_DELAY:

        if (pData && Header.m_DataLength <= 2)
            g_LightDelay = *reinterpret_cast<uint16_t *>(pData);
        else
            Serial.println("E:Bad Delay Data");
        break;
    case SERIN_GET_DELAY:

        Serial.print("D:5:");
        Serial.print(Header.m_Id);
        Serial.print(":");
        Serial.println(g_LightDelay);
        break;
    }

    if (pData != nullptr)
        delete[] pData;

    delay(500);
}