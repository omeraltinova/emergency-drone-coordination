# Emergency Drone Coordination System: Project Report

## 1. Introduction
Bu proje, acil durumlarda hayatta kalanları tespit etmek ve yardım ulaştırmak için otonom droneların bir sunucu ile koordinasyonunu sağlar. Proje üç ana faza ayrılmıştır:

- **Phase 0 (Lab 10)**: Temel TCP çoklu iş parçacıklı sunucu ve istemci şablonu.
- **Phase 1**: Handshake, periyodik durum güncellemesi ve görev atama.
- **Phase 2**: Nabız (heartbeat) mekanizması ve yeniden bağlanma/diskonekt kontrolü.

## 2. Phase 0: Temel Mimari
- Çoklu iş parçacıklı TCP sunucu şablonu.
- SDL ile GUI desteği (özellikle macOS).  
- List yapıları ve temel mesajlaşma altyapısı.

## 3. Phase 1: Handshake & Görev Yönetimi
- **Handshake**: Drone → Sunucu, `HANDSHAKE` mesajı, ardından sunucudan `HANDSHAKE_ACK`.
- **STATUS_UPDATE**: Periyodik konum, hız, batarya ve durum bilgisi iletim.
- **ASSIGN_MISSION**: Sunucu → Drone, hedef koordinat ve öncelik.
- **MISSION_COMPLETE**: Drone’dan sunucuya görev tamamlandığını bildiren mesaj.

## 4. Phase 2: Heartbeat & Güvenilirlik
- **HEARTBEAT**: Sunucudan dronelara periyodik `HEARTBEAT` mesajı.
- **HEARTBEAT_RESPONSE**: Drone’dan sunucuya yanıt.  
- **Disconnection Handling**:
  - 3 kaçırılan nabızda drone listeden tamamen kaldırılır.
  - Anlık kopmalarda 25s içinde yeniden bağlanma imkânı.
  - Tüm bağlantısızlıklarda sunucu crash olmadan çalışmaya devam eder.
- **Watchdog**: 60s boyunca hiçbir drone’dan mesaj alınamazsa sunucu kendini kapatır.

## 5. İletişim Protokolü
| Yö­n­tem              | Tip                | Amaç                                       |
|---------------------|--------------------|--------------------------------------------|
| Drone → Sunucu      | HANDSHAKE          | Kayıt                                      |
|                     | STATUS_UPDATE      | Konum, durum, batarya                     |
|                     | MISSION_COMPLETE   | Görev tamamlandı bildirimi                 |
|                     | HEARTBEAT_RESPONSE | Nabız yanıtı                               |
| Sunucu → Drone      | HANDSHAKE_ACK      | Kayıt onayı                                |
|                     | ASSIGN_MISSION     | Yeni görev atama                           |
|                     | HEARTBEAT          | Canlılık denetimi                          |
| Her İki Yö­n­tem    | ERROR              | Hata bildirimleri                          |

Mesaj örnekleri ve JSON şemaları `communication-protocol.md` dosyasında detaylıdır.

## 6. Uygulama Detayları
- cJSON kütüphanesi ile JSON işlemleri.
- pthread ile iş parçacıklı mesaj işleme ve izleme.
- SDL2 ile harita ve durum görselleştirme.
- Kilitler (mutex), koşul değişkenleri (condvar) ve zamanlayıcılarla eş zamanlılık.

## 7. Gelecek Çalışmalar
- Görev önceliklendirme ve dinamik kuyruk yönetimi.
- Gerçek dünya harita verisi ve merkezi bir yönlendirme algoritması (örneğin A*).  
- Drone batarya tüketimine göre planlama iyileştirmeleri.

## 8. Kütüphaneler ve Bağımlılıklar
- **cJSON**: Hafif C JSON kütüphanesi. Sunucu ve drone istemcisi arasındaki JSON mesajlaşmayı sağlar.
- **pthread**: Çoklu iş parçacığı (multithreading) desteği. Her drone için ayrı `client_handler`, nabız ve watchdog thread’leri.
- **SDL2**: Harita ve droneların durumunu görselleştirmek için grafik API’si.
- **Standard C kütüphaneleri**: `time.h`, `errno.h`, `arpa/inet.h` vb.

## 9. JSON Mesaj Örnekleri
### 9.1 Drone → Sunucu
**HANDSHAKE**
```json
{
  "type": "HANDSHAKE",
  "drone_id": "D1"
}
```
**STATUS_UPDATE**
```json
{
  "type": "STATUS_UPDATE",
  "drone_id": "D1",
  "timestamp": 1620000000,
  "location": {"x":10,"y":20},
  "status": "idle",
  "battery": 85,
  "speed": 5
}
```
**MISSION_COMPLETE**
```json
{
  "type": "MISSION_COMPLETE",
  "drone_id": "D1",
  "mission_id": "M123",
  "timestamp": 1620000000,
  "success": true,
  "details": "Delivered aid"
}
```
**HEARTBEAT_RESPONSE**
```json
{
  "type": "HEARTBEAT_RESPONSE",
  "drone_id": "D1",
  "timestamp": 1620000000
}
```

### 9.2 Sunucu → Drone
**HANDSHAKE_ACK**
```json
{
  "type": "HANDSHAKE_ACK",
  "session_id": "S1",
  "config": {"status_update_interval":5,"heartbeat_interval":10}
}
```
**ASSIGN_MISSION**
```json
{
  "type": "ASSIGN_MISSION",
  "mission_id": "M123",
  "priority": "high",
  "target": {"x":18,"y":5}
}
```
**HEARTBEAT**
```json
{
  "type": "HEARTBEAT",
  "timestamp": 1620000000
}
```

## 10. Tasarım Kararları ve Bağlantı Yönetimi
- **Heartbeats**: Dronelerin canlılığını 3 gözlemde izleyip, kaçırılan nabızlarda listeden çıkarmak için.  
- **Reconnection Window**: Anlık ağ kopmalarına karşı 25s içinde tekrar bağlanma imkânı.
- **Watchdog**: 60s boyunca hiç mesaj alınmazsa sunucu kendini kapatır—gereksiz kaynak tüketimini önlemek için.

## 11. Arayüz (GUI) Kullanımı
1. `make && ./launcher` ile başlatın.
2. Sunucuyu "Launch Server" tuşu ile çalıştırın.
3. SDL penceresinde harita görüntülenir; dronelar ve kurtarılan koordinatlar renklerle işaretlenir.
4. Pencereyi kapatmak veya ESC tuşuna basmak sunucuyu düzgün kapatır.

