"""macOS 를 BLE peripheral 로 띄운다 — NUS + DIS + 배터리.

central 시험용 상대가 필요할 때 보드 두 대 대신 쓴다.
CoreBluetooth 는 광고에 local name 과 service UUID 만 실을 수 있는데,
central 이 NUS UUID 로 걸러 찾는 데는 그걸로 충분하다.
"""
import sys, time
import objc
from Foundation import NSObject, NSData, NSRunLoop, NSDate
from CoreBluetooth import (
    CBPeripheralManager, CBMutableService, CBMutableCharacteristic, CBUUID,
    CBAdvertisementDataLocalNameKey, CBAdvertisementDataServiceUUIDsKey,
)

NUS   = CBUUID.UUIDWithString_("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
NUS_RX= CBUUID.UUIDWithString_("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")  # central -> us
NUS_TX= CBUUID.UUIDWithString_("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")  # us -> central
DIS   = CBUUID.UUIDWithString_("180A")
BAS   = CBUUID.UUIDWithString_("180F")

P_READ, P_WRITE_NR, P_WRITE, P_NOTIFY = 0x02, 0x04, 0x08, 0x10
PERM_READ, PERM_WRITE = 0x01, 0x02
DURATION = float(sys.argv[1]) if len(sys.argv) > 1 else 40

def d(s): return NSData.dataWithBytes_length_(s, len(s))

class Delegate(NSObject):
    def init(self):
        self = objc.super(Delegate, self).init()
        self.txc = None; self.mgr = None; self.subscribed = False; self.added = 0
        return self

    def peripheralManagerDidUpdateState_(self, mgr):
        if mgr.state() != 5:
            print("bluetooth not powered on (state=%d)" % mgr.state()); return
        self.mgr = mgr
        self.txc = CBMutableCharacteristic.alloc().initWithType_properties_value_permissions_(
            NUS_TX, P_NOTIFY, None, PERM_READ)
        rxc = CBMutableCharacteristic.alloc().initWithType_properties_value_permissions_(
            NUS_RX, P_WRITE | P_WRITE_NR, None, PERM_WRITE)
        nus = CBMutableService.alloc().initWithType_primary_(NUS, True)
        nus.setCharacteristics_([self.txc, rxc])

        # macOS 는 DIS(180A) / BAS(180F) 같은 예약 서비스의 게시를 거부한다
        # (CBError 8 "UUID is not allowed"). NUS 만 올린다 — central 시험에는 충분하다.
        mgr.addService_(nus)

    def peripheralManager_didAddService_error_(self, mgr, svc, err):
        if err: print("addService error:", err); return
        self.added += 1
        if self.added == 1:
            mgr.startAdvertising_({CBAdvertisementDataLocalNameKey: "BARAM Mac",
                                   CBAdvertisementDataServiceUUIDsKey: [NUS]})

    def peripheralManagerDidStartAdvertising_error_(self, mgr, err):
        print("advertising" if not err else "advertise error: %s" % err, flush=True)

    def peripheralManager_central_didSubscribeToCharacteristic_(self, mgr, central, chr):
        print("central subscribed to TX", flush=True); self.subscribed = True

    def peripheralManager_didReceiveWriteRequests_(self, mgr, reqs):
        for r in reqs:
            v = bytes(r.value()) if r.value() else b""
            print("  <- from central: %r" % v, flush=True)
        mgr.respondToRequest_withResult_(reqs[0], 0)

dele = Delegate.alloc().init()
mgr = CBPeripheralManager.alloc().initWithDelegate_queue_(dele, None)
t0 = time.time(); n = 0
while time.time() - t0 < DURATION:
    NSRunLoop.currentRunLoop().runUntilDate_(NSDate.dateWithTimeIntervalSinceNow_(0.4))
    if dele.subscribed and dele.txc is not None and time.time() - t0 > n * 2 + 2:
        n += 1
        msg = ("mac-tick-%d" % n).encode()
        ok = mgr.updateValue_forCharacteristic_onSubscribedCentrals_(d(msg), dele.txc, None)
        print("  -> notify %r (%s)" % (msg, "sent" if ok else "queued"), flush=True)
print("done")
