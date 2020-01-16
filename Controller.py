import sys
import serial

import struct

import io

import time
import threading

import requests

import json


def GetTimeMillis(): return int(round(time.time() * 1000))


c_BAUD = 9600

c_GetUrl = "[REDACTED]"  # This is the command get url
c_PostUrl = "[REDACTED]" # This is the result post url



g_IsRunnning = False


class SerialPorts:
    def __init__(self):

        self.m_AvaliblePorts = []

        PossiblePorts = self.GetPossiblePorts()

        for Port in PossiblePorts:
            self.CheckPortAvalible(Port)

    def GetAvaliblePorts(self):
        return self.m_AvaliblePorts

    def CheckPortAvalible(self, port):

        try:

            SerialCom = serial.Serial(port)
            SerialCom.close()

            self.m_AvaliblePorts.append(port)

        except (OSError):
            pass

    def GetPossiblePorts(self):
        Ports = []

        if sys.platform.startswith('win'):
            Ports = ['COM%s' % (i + 1) for i in range(256)]
        else:
            raise EnvironmentError('Unsupported platform')

        return Ports


class Serial:

    def __init__(self, port):
        self.m_Port = port
        self.m_Serial = None

    def HasSerial(self):
        return self.m_Serial != None

    def ConnectSerial(self):
        self.m_Serial = serial.Serial(self.m_Port, c_BAUD)

    def UpdateSerial(self):

        if not self.HasSerial():
            self.ConnectSerial()

        return self.HasSerial()

    def GetSerial(self):
        return self.m_Serial

    def SendCommand(self, commandId, payload=None, id=0):

        global g_IsRunnning

        if not self.UpdateSerial():
            print("No serial")
            return

        g_IsRunnning = True

        Data = bytearray(8)

        PayLoadSize = 0
        if payload != None:
            PayLoadSize = len(payload)

        struct.pack_into("<HHHH", Data, 0, 0x89af, id, commandId, PayLoadSize)

        if payload != None:
            Data.extend(payload)

        print(Data)

        self.m_Serial.write(Data)


def ProcessType1(lineValues):
    return {
        "h": lineValues[0],
        "result": lineValues[1]
    }


def ProcessType2(lineValues):
    return {
        "b": lineValues[0],
        "result": lineValues[1]
    }


def ProcessType3(lineValues):
    return {
        "r": lineValues[0],
        "g": lineValues[1],
        "b": lineValues[2],
        "result": lineValues[3]
    }


def ProcessType(dataType, lineArray):
    DataLineArray = [

    ]

    for Line in lineArray:
        SplitLineData = []

        SplitData = Line.m_Data.split(",")

        print()

        for Split in SplitData:
            SplitLineData.append(int(Split))

        if dataType == 1:
            DataLineArray.append(ProcessType1(SplitLineData))
        elif dataType == 2:
            DataLineArray.append(ProcessType2(SplitLineData))
        elif dataType == 3:
            DataLineArray.append(ProcessType3(SplitLineData))

    return DataLineArray



class DataProcessor:
    class LineData:
        def __init__(self, id, dataType, data):
            self.m_Id = id
            self.m_Type = dataType
            self.m_Data = data

    def __init__(self):
        self.m_LineData = []
        return

    def ProcessData(self):

        Id = -1
        Type = -1

        DataArray = []

        for Line in self.m_LineData:

            if Type == -1:
                Type = Line.m_Type

            if Id == -1:
                Id = Line.m_Id

            if Line.m_Id != Id or Line.m_Type != Type:
                break

            DataArray.append(Line)

        if Id == -1:
            return

        if Type == -1:
            return

        if len(DataArray) == 0:
            return

        # Remove line data, so next processing frame can get the next type...
        self.m_LineData = [x for x in self.m_LineData if x not in DataArray]

        Data = ProcessType(Type, DataArray)

        ResultData = {
            "type": Type,
            "id": Id,
            "data":  Data,
        }

        print("%s" % (json.dumps(ResultData)))

        try:
            
            Response = requests.post(c_PostUrl, data=json.dumps(ResultData), headers={"Content-Type": "text/plain"})

            print("Result %s" % Response.text)
            print(Response.status_code)

        except:
            print("Cant Post!")
            return

    def AddLine(self, lineData):

        Type = -1
        Id = -1
        LineValueData = None

        LineDataSplit = lineData.split(":")

        print(LineDataSplit)

        if LineDataSplit[0] == "E":
            print("ERROR: %s", LineDataSplit[1])
            return
        elif LineDataSplit[0] == "D":
            Type = int(LineDataSplit[1])
            Id = int(LineDataSplit[2])
            LineValueData = LineDataSplit[3]

        self.m_LineData.append(DataProcessor.LineData(Id, Type, LineValueData))

        print(LineValueData)


class ReadThread(threading.Thread):

    def __init__(self, com, processor):
        super(ReadThread, self).__init__()
        self.m_Com = com
        self.m_Processor = processor
        self.m_LastReceivedTime = 0

    def run(self):
        global g_IsRunnning

        TempData = ""

        while True:

            if self.m_Com.GetSerial() == None:
                continue

            Length = self.m_Com.GetSerial().inWaiting()

            if Length == 0 and self.m_LastReceivedTime != 0:

                if (GetTimeMillis() - self.m_LastReceivedTime) > 1000:

                    self.m_LastReceivedTime = 0
                    g_IsRunnning = False
                    self.m_Processor.ProcessData()
                    
            if Length > 0:

                self.m_LastReceivedTime = GetTimeMillis()
                DataString = self.m_Com.GetSerial().read(Length).decode('ascii')

                if TempData != "":
                    if "\n" in DataString:
                        SplitData = DataString.split("\n")

                        DataString = TempData + SplitData[0]
                        TempData = SplitData[1]

                    else:
                        TempData += DataString
                        continue

                elif "\n" in DataString:
                    SplitData = DataString.split("\n")
                    DataString = SplitData[0]
                else:
                    TempData += DataString
                    continue

                self.m_Processor.AddLine(DataString)


def ProcessInputJson(serial, inputData):

    Id = -1

    Type = -1

    PayloadArray = None

    if "type" in inputData:
        Type = int(inputData["type"])

    if "id" in inputData:
        Id = int(inputData["id"])

    if "payload" in inputData:
        PayloadArray = bytearray()

        for Byte in inputData["payload"]:
            PayloadArray.append(Byte)

    if Type == -1:
        return

    if Type == 0:
        return

    print("Sending type %i" % Type)

    if Id == -1:
        serial.SendCommand(Type, PayloadArray)
    else:
        serial.SendCommand(Type, PayloadArray, Id)


def Main():
    global g_IsRunnning

    AvaliblePort = SerialPorts()

    Ports = AvaliblePort.GetAvaliblePorts()

    if len(Ports) == 0:
        print("No Serial ports avalible")
        return

    print(Ports)

    print("Connecting to port %s" % Ports[0])

    Commmunication = Serial(Ports[0])

    Commmunication.ConnectSerial()

    time.sleep(3)

    Processor = DataProcessor()

    ListenThread = ReadThread(Commmunication, Processor)
    ListenThread.daemon = True

    ListenThread.start()


    LastCheckTime = 0 
    while (True):

        if g_IsRunnning:
            continue

        if LastCheckTime != 0 and (GetTimeMillis() - LastCheckTime) < 2000:
            continue
        
        LastCheckTime = GetTimeMillis()

        print("Trying to get command")
        try:
            ResultRequest = requests.get(c_GetUrl)
        except:
            continue

        if ResultRequest.status_code != 200:
            print("Cant get url! [%i]" % (ResultRequest.status_code))
            continue

        ResultJson = json.loads(ResultRequest.text)

        ProcessInputJson(Commmunication, ResultJson)


if __name__ == "__main__":
    Main()
