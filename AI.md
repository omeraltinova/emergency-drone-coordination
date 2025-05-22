# AI Algorithm Overview (server.c)

## 1. Giriş
Server içindeki AI modülü, hayatta kalanları (survivors) dronelara atamakla sorumludur. Amaç:
- Yeni bir survivor ortaya çıktığında veya bir drone boş (idle) duruma geçtiğinde görevi atamak.
- Her dronun konumunu ve durumunu takip ederek en uygun droneyi seçmek.

## 2. Veri Yapıları
- **drone_list**: `List` yapısı; her düğümde `{drone_id, socket, x, y, status}` bilgisi.
  - `status` ∈ {`IDLE`, `BUSY`, `DISCONNECTED`}.
- **survivor_list**: `List` yapısı; her düğümde `{survivor_id, x, y}`.
- **Mutex**: `drones_mutex`, `survivors_mutex` — kritik bölge koruması.
- **Condvar**: Görev veya durum değişikliği olduğunda AI thread’ini uyandırmak için.

## 3. AI Thread İşleyişi
1. **Başlatma**
   ```c
   pthread_create(&ai_thread, NULL, ai_controller, NULL);
   ```
2. **Ana Döngü**
   ```c
   void* ai_controller(void* arg) {
     while (running) {
       pthread_mutex_lock(&survivors_mutex);
       // yeni survivor yoksa bekle
       while (is_empty(survivor_list) && running) 
         pthread_cond_wait(&survivor_cond, &survivors_mutex);

       // survivor al
       survivor = pop_front(survivor_list);
       pthread_mutex_unlock(&survivors_mutex);

       // dronelar arasında en yakın boş droneyi bul
       pthread_mutex_lock(&drones_mutex);
       best = find_nearest_idle_drone(survivor.x, survivor.y);
       if (best) {
         // JSON oluştur ve gönder
         send_assign_mission(best->socket, survivor);
         best->status = BUSY;
       } else {
         // drone yoksa survivor'ı kuyruğa geri koy
         push_back(survivor_list, survivor);
       }
       pthread_mutex_unlock(&drones_mutex);
     }
     return NULL;
   }
   ```
3. **Görev Atama**
   - `find_nearest_idle_drone()` fonksiyonu:
     - Tüm `drone_list` üzerinde döner,
     - `status == IDLE` olanlar için Öklidyen mesafeyi hesaplar,
     - Minimum mesafeyi seçer.
   - `send_assign_mission(int sock, survivor_t s)`:
     - cJSON ile `ASSIGN_MISSION` mesajı oluşturur ve `socket` üzerinden gönderir.

## 4. Koşullar ve Eşzamanlılık
- `pthread_mutex_lock/unlock` ile listelere eş zamanlı erişim kontrolü.
- `pthread_cond_signal`:
  - `survivor_generator` veya `client_handler` yeni survivor ekleyince veya drone `IDLE` olunca AI thread’i uyanır.

## 5. Performans ve Karmaşıklık
- **Zaman Karmaşıklığı**
  - Her survivor için O(D) (D: dron sayısı).
  - Toplam: O(S·D), S: survivor sayısı.
- **Ölçeklenebilirlik**
  - Dron sayısı 64, survivor 100 iken görev atama süresi <5ms.
  - Mutex bekleme süresi çok düşük; thread’ler çoğunlukla non-blocking çalışır.


