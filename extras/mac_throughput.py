"""맥을 central 로 띄워 보드의 BLE 처리량을 잰다.

  python3 extras/mac_throughput.py [초]

examples/Peripheral/throughput 를 구운 보드와 짝이다. 붙어서 알림을 켜고,
"시작" 바이트 하나를 보낸 뒤 들어오는 양을 센다. 그다음 반대 방향으로
write-without-response 를 쏟아 부어 업링크도 잰다.

⚠ CoreBluetooth 는 PHY 도 연결 간격도 노출하지 않는다. 맥이 알아서 정하고,
  실제로 무엇으로 정해졌는지는 **보드 쪽 시리얼 출력**에만 나온다. 두 수치를
  같이 봐야 한다.

⚠ 호스트가 GATT 를 캐시한다. 펌웨어의 서비스 구성을 바꿨는데 서비스가 덜
  보이면 시스템 설정에서 그 기기를 지우고 다시 붙어라.
"""
import sys, time, functools
import objc
from Foundation import NSObject, NSData, NSRunLoop, NSDate
from CoreBluetooth import CBCentralManager, CBUUID

print = functools.partial(print, flush=True)

NUS    = CBUUID.UUIDWithString_("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
NUS_RX = CBUUID.UUIDWithString_("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")  # 우리 -> 보드
NUS_TX = CBUUID.UUIDWithString_("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")  # 보드 -> 우리

WRITE_WITH_RESPONSE, WRITE_WITHOUT_RESPONSE = 0, 1
DURATION = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0

# 다운링크가 이만큼 조용하면 끝난 것으로 본다.
IDLE_DONE_S = 1.5


class Central(NSObject):
    def init(self):
        self = objc.super(Central, self).init()
        self.p = self.rx = self.tx = None
        self.n = 0
        self.t0 = self.tlast = None
        self.down_done = False
        self.up_bytes = 0
        self.up_ms = 0
        return self

    # ── 연결 ────────────────────────────────────────────────────────────
    def centralManagerDidUpdateState_(self, m):
        if m.state() == 5:
            print("[scan] NUS 를 찾는다")
            m.scanForPeripheralsWithServices_options_([NUS], None)

    def centralManager_didDiscoverPeripheral_advertisementData_RSSI_(self, m, p, a, r):
        if self.p:
            return
        print("[scan] %s (RSSI %s)" % (p.name(), r))
        m.stopScan()
        self.p = p
        p.setDelegate_(self)
        m.connectPeripheral_options_(p, None)

    def centralManager_didConnectPeripheral_(self, m, p):
        p.discoverServices_(None)

    def peripheral_didDiscoverServices_(self, p, e):
        for s in (p.services() or []):
            if s.UUID() == NUS:
                p.discoverCharacteristics_forService_(None, s)

    def peripheral_didDiscoverCharacteristicsForService_error_(self, p, s, e):
        for c in (s.characteristics() or []):
            if c.UUID() == NUS_TX:
                self.tx = c
            elif c.UUID() == NUS_RX:
                self.rx = c

        if self.tx is None or self.rx is None:
            print("[gatt] NUS characteristic 을 못 찾았다 — 호스트 캐시일 수 있다")
            return

        # 맥이 한 번에 실을 수 있는 양. 협상된 MTU - 3 이다.
        self.mtu = p.maximumWriteValueLengthForType_(WRITE_WITHOUT_RESPONSE)
        print("[gatt] 준비됨. 맥의 write 최대 %d 바이트" % self.mtu)
        p.setNotifyValue_forCharacteristic_(True, self.tx)

    def peripheral_didUpdateNotificationStateForCharacteristic_error_(self, p, c, e):
        print("[down] 시작 신호를 보낸다")
        b = b"g"
        p.writeValue_forCharacteristic_type_(
            NSData.dataWithBytes_length_(b, len(b)), self.rx, WRITE_WITHOUT_RESPONSE)

    # ── 다운링크 (보드 -> 맥) ───────────────────────────────────────────
    def peripheral_didUpdateValueForCharacteristic_error_(self, p, c, e):
        v = c.value()
        if not v:
            return
        now = time.time()
        if self.t0 is None:
            self.t0 = now
        self.tlast = now
        self.n += len(bytes(v))


def report(label, nbytes, seconds):
    if nbytes == 0 or seconds <= 0:
        print("%-22s 측정 못 함" % label)
        return
    kbs = nbytes / 1000.0 / seconds
    print("%-22s %8d 바이트 / %5.2f 초 = %6.2f KB/s (%.0f kbps)"
          % (label, nbytes, seconds, kbs, kbs * 8))


d = Central.alloc().init()
mgr = CBCentralManager.alloc().initWithDelegate_queue_(d, None)

# 1) 다운링크를 잰다.
end = time.time() + DURATION
while time.time() < end:
    NSRunLoop.currentRunLoop().runUntilDate_(NSDate.dateWithTimeIntervalSinceNow_(0.05))
    if d.tlast and (time.time() - d.tlast) > IDLE_DONE_S:
        break

down_s = (d.tlast - d.t0) if (d.t0 and d.tlast and d.tlast > d.t0) else 0

# 2) 업링크 — 맥이 보드로 쏟아 붓는다.
if d.rx is not None and d.p is not None:
    chunk = b"1" * d.mtu
    print("[up] %d 바이트씩 5 초 동안 보낸다" % d.mtu)
    sent = 0
    t0 = time.time()
    while time.time() - t0 < 5.0:
        # 큐가 찼으면 CoreBluetooth 가 받아 주지 않으므로 물어보고 넣는다.
        if d.p.canSendWriteWithoutResponse():
            d.p.writeValue_forCharacteristic_type_(
                NSData.dataWithBytes_length_(chunk, len(chunk)), d.rx, WRITE_WITHOUT_RESPONSE)
            sent += len(chunk)
        NSRunLoop.currentRunLoop().runUntilDate_(NSDate.dateWithTimeIntervalSinceNow_(0.001))
    d.up_bytes, d.up_ms = sent, time.time() - t0

print("\n=== 결과 ===")
report("다운링크 (보드->맥)", d.n, down_s)
report("업링크   (맥->보드)", d.up_bytes, d.up_ms)
print("\nPHY / MTU / 연결간격은 보드 쪽 시리얼 출력을 보라 — 맥은 노출하지 않는다.")
