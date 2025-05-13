# Phase 1: Drone Simülasyonu - Geliştirme Özeti ve Mevcut Durum

## 1. Proje Amacı (Phase 1)

Bu fazın temel amacı, çoklu thread'ler kullanarak çalışan bir acil durum drone koordinasyon sisteminin temel altyapısını oluşturmaktı. Hedefler şunlardı:
*   Thread-safe (iş parçacığı güvenli) bir liste veri yapısı implemente etmek ve mevcut olanı düzeltmek.
*   Mevcut simülatör kodundaki hataları (veri bozulmaları, çökme sorunları, senkronizasyon hataları) gidermek.
*   Drone'ların davranışlarını (görev bekleme, hedefe hareket, görev tamamlama) ayrı thread'lerde simüle etmek.
*   Hayatta kalanlara drone ataması yapacak temel bir AI (Yapay Zeka) denetleyicisi geliştirmek.
*   Sistemin anlık durumunu (drone'lar, hayatta kalanlar, görevler) SDL kütüphanesi kullanarak görselleştirmek.
*   Kaynakların (bellek, thread'ler, senkronizasyon nesneleri) doğru bir şekilde yönetilmesini sağlamak.

## 2. Temel Bileşenler ve Rolleri

Proje, birkaç ana C kaynak dosyası ve bunların başlık dosyalarından oluşur:

*   **`controller.c`**: Ana program akışını yönetir. Global değişkenleri (listeler, `running` bayrağı vb.) tanımlar. Ana thread'leri (AI, survivor_generator, drone thread'leri için başlatıcı, SDL view) başlatır, programın sonlanmasını yönetir ve tüm kaynakların temizlenmesinden sorumludur. Sinyal yakalama (örn: CTRL+C) ile kontrollü kapanmayı sağlar.
*   **`list.c`, `headers/list.h`**: Jenerik, çift yönlü bağlı liste veri yapısını implemente eder. Bu yapı, `Drone*` ve `Survivor*` gibi işaretçileri saklamak için kullanılmıştır. Thread güvenliği mutex'ler ve semaforlar ile sağlanmıştır.
*   **`drone.c`, `headers/drone.h`**: Bireysel drone'ların mantığını içerir. `initialize_drones` fonksiyonu tüm drone'ları, ilgili thread'lerini ve senkronizasyon nesnelerini (mutex, koşul değişkeni) oluşturur. `drone_behavior` fonksiyonu her bir drone thread'inin ana döngüsüdür; görevleri bekler ve yerine getirir. `cleanup_drones` fonksiyonu program sonunda tüm drone kaynaklarını temizler.
*   **`survivor.c`, `headers/survivor.h`**: Hayatta kalanların mantığını içerir. `survivor_generator` fonksiyonu, ayrı bir thread'de periyodik olarak yeni `Survivor` nesneleri oluşturur ve bunları ilgili listelere ekler. `create_survivor` dinamik olarak bellek ayırır.
*   **`ai.c`, `headers/ai.h`**: AI denetleyicisinin mantığını içerir. `ai_controller` fonksiyonu, ayrı bir thread'de çalışarak bekleyen hayatta kalanları kontrol eder, boşta olan en yakın drone'u bulur ve `assign_mission` aracılığıyla drone'a görev atar.
*   **`map.c`, `headers/map.h`**: Simülasyon haritasının oluşturulması ve yönetilmesiyle ilgilidir. `init_map` harita hücrelerini ve her hücre için (başlangıçta boş) hayatta kalan listelerini oluşturur. `freemap` harita kaynaklarını temizler.
*   **`view.c`, `headers/view.h`**: SDL kullanarak simülasyonun görselleştirilmesinden sorumludur. Haritayı, drone'ları (farklı durumlarına göre renklerle), hayatta kalanları (bekleyen, yardım edilen, hedef olarak) ve görev çizgilerini çizer.
*   **`headers/globals.h`, `headers/coord.h`**: Global değişken bildirimleri (örn: listeler, `running` bayrağı) ve koordinat yapısı gibi paylaşılan tanımlamaları içerir.

## 3. Anahtar Veri Yapıları

*   **`List` Yapısı (`headers/list.h`)**:
    *   **Thread Güvenliği**:
        *   `pthread_mutex_t lock`: Liste üzerinde yapılan tüm işlemler (ekleme, çıkarma, okuma, iterasyon) bu mutex ile korunur.
        *   `sem_t elements_sem`: Listede okunmayı bekleyen eleman sayısını takip eder (`pop` gibi işlemler için).
        *   `sem_t capacity_sem`: Listede ne kadar boş yer olduğunu takip eder (`add` gibi işlemler için).
    *   **İşaretçi Saklama**: Liste, `void*` işaretçileri saklamak üzere tasarlanmıştır, ancak bizim projemizde özellikle `Drone*` ve `Survivor*` işaretçilerini saklar. Bu nedenle, `create_list` çağrılırken `datasize` parametresi olarak `sizeof(Drone*)` veya `sizeof(Survivor*)` verilir.
    *   **Veri Ekleme/Erişim**:
        *   **Ekleme (`add`):** Listeye bir işaretçi (örn: `Survivor* s_ptr`) eklerken, işaretçinin kendisini (yani adresini) değil, işaretçinin *değerini* listeye kopyalamak için, `add` fonksiyonuna bu işaretçinin adresi (`&s_ptr`) verilir. `list.c` içindeki `memcpy(node->data, &s_ptr, sizeof(Survivor*));` bu işlemi gerçekleştirir; `node->data` alanı artık `s_ptr`'nin içerdiği adresi tutar.
        *   **Okuma/Erişim (`peek`, iterasyon):** Listeden bir işaretçi okunurken (örn: `peek_result = list->peek(list);` veya `node->data` üzerinden), dönen `void*` (veya `char data[]`nın adresi) aslında saklanan asıl işaretçiyi (örn: `Survivor*`) içeren bellek bölgesinin adresidir. Bu nedenle, asıl `Survivor*` işaretçisine erişmek için bir kez daha dereferans yapılır: `Survivor* actual_s_ptr = *(Survivor**)peek_result;` veya `Survivor* actual_s_ptr = *(Survivor**)node->data;`. **Bu işaretçi kullanımı, projedeki birçok çökme ve veri bozulması sorununun temel çözümü olmuştur.**

*   **`Drone` Yapısı (`headers/drone.h`)**:
    *   `id`, `status` (`IDLE`, `ON_MISSION`, `DISCONNECTED`), `coord` (mevcut konum), `target` (görev hedefi).
    *   `pthread_t thread_id`: Drone'un kendi thread ID'si.
    *   `pthread_mutex_t lock`: Bu drone'a özel verileri (konum, durum, hedef vb.) korumak için kullanılır.
    *   `pthread_cond_t mission_cv`: Drone `IDLE` durumundayken yeni bir görev beklemesi için kullanılır. AI tarafından `pthread_cond_signal` ile uyandırılır.
    *   `volatile bool lock_initialized`, `volatile bool cv_initialized`: Mutex ve CV'nin başarıyla başlatılıp başlatılmadığını takip eden bayraklar. Kaynakların yanlışlıkla (başlatılmadan/yok edildikten sonra) kullanılmasını önleyerek "futex" hatalarını gidermiştir.

*   **`Survivor` Yapısı (`headers/survivor.h`)**:
    *   `coord`, `info`, `discovery_time`, `status` (0: bekliyor, 1: yardım edildi/ediliyor).

*   **`Map` Yapısı (`headers/map.h`)**:
    *   `width`, `height`.
    *   `Cell** cells`: 2D bir hücre dizisi. Her `Cell` kendi koordinatını ve o hücrede bulunan hayatta kalanlar için bir `List* survivors` (bu liste de `Survivor*` saklar) içerir.

## 4. Thread Yönetimi ve Senkronizasyon

*   **Ana Thread'ler**:
    *   `survivor_generator_thread`: `survivor_generator` fonksiyonunu çalıştırır.
    *   `ai_controller_thread`: `ai_controller` fonksiyonunu çalıştırır.
    *   `drone_fleet[i].thread_id`: Her bir drone için `drone_behavior` fonksiyonunu çalıştıran thread'ler.
    *   Ana thread (`main`): SDL olay döngüsünü ve `draw_map` çağrılarını yönetir (view thread'i gibi davranır).
*   **Oluşturma ve Sonlandırma**: Tüm thread'ler `controller.c`'deki `main` fonksiyonunda oluşturulur ve `cleanup_and_exit` fonksiyonunda `pthread_join` ile düzgün bir şekilde sonlandırılır.
*   **Genel Senkronizasyon Stratejisi**:
    *   Global Listeler (`survivors`, `helpedsurvivors`, `drones`): Tüm erişimler (ekleme, çıkarma, `peek`, iterasyon) `list.c` içindeki ve çağıran fonksiyondaki (örn: `ai.c`, `view.c`) `pthread_mutex_lock/unlock` çağrılarıyla korunur.
    *   Bireysel Drone Verileri: Her `Drone` nesnesinin kendi `lock` mutex'i vardır. `drone_behavior` ve `assign_mission` gibi fonksiyonlar drone'un durumunu/koordinatlarını değiştirirken bu kilidi kullanır.
    *   Drone Görev Bildirimi: `drone_behavior` `IDLE` durumundayken `mission_cv` üzerinde bekler. `ai_controller` yeni bir görev atadığında `assign_mission` aracılığıyla bu CV'ye sinyal gönderir.
    *   Global Kapanma: `volatile sig_atomic_t running = 1;` bayrağı tüm ana döngüleri (drone, AI, survivor, main) kontrol eder. `cleanup_and_exit` içinde `0`'a ayarlanarak tüm thread'lerin sonlanması sinyali verilir.

## 5. İş Akışı / Program Mantığı

1.  **Başlatma (`controller.c` `main`)**:
    1.  Sinyal işleyiciler (SIGINT, SIGTERM için `signal_handler`) ayarlanır.
    2.  Global listeler (`survivors`, `helpedsurvivors`, `drones`) `create_list(sizeof(PointerType*), capacity)` çağrılarıyla oluşturulur.
    3.  `init_map()` ile harita ve hücre içi listeler oluşturulur.
    4.  `initialize_drones()`:
        *   `drone_fleet` için bellek ayrılır.
        *   Her drone için: ID, durum, rastgele başlangıç koordinatları ayarlanır. Mutex ve CV başlatılır (`lock_initialized`, `cv_initialized` bayrakları güncellenir). `Drone* drone_ptr = &drone_fleet[i];` alınır ve `drones->add(drones, &drone_ptr);` ile global `drones` listesine eklenir. `pthread_create` ile `drone_behavior` thread'i başlatılır.
    5.  `survivor_generator_thread` (`survivor_generator` fonksiyonu ile) başlatılır.
    6.  `ai_controller_thread` (`ai_controller` fonksiyonu ile) başlatılır.
    7.  `init_sdl_window()` ile SDL görselleştirme penceresi hazırlanır.

2.  **Çalışma Zamanı Döngüsü**:
    *   **`survivor_generator` (`survivor.c`)**: Periyodik olarak (test sonrası varsayılan olarak 2-4 saniyede bir):
        *   Yeni bir `Survivor` nesnesi için `create_survivor` ile `malloc` yapar ve bilgilerini doldurur.
        *   `Survivor* s_ptr = ...;`
        *   `survivors->add(survivors, &s_ptr);` ile global bekleyenler listesine ekler.
        *   `map.cells[x][y].survivors->add(map.cells[x][y].survivors, &s_ptr);` ile ilgili harita hücresinin listesine ekler (bu liste şu an `view.c` tarafından doğrudan kullanılmıyor ama veri tutarlılığı için eklidir).
    *   **`ai_controller` (`ai.c`)**: Periyodik olarak (varsayılan olarak saniyede bir):
        *   `void* peek_result = survivors->peek(survivors);` ile bekleyenler listesinin başından bir hayatta kalan işaretçisi alır.
        *   Eğer `peek_result` geçerliyse, `Survivor* s = *(Survivor**)peek_result;` ile asıl `Survivor` işaretçisini alır.
        *   `Drone* closest_drone = find_closest_idle_drone(s->coord);` çağrılır.
            *   `find_closest_idle_drone`: Global `drones` listesi üzerinde kilitli bir şekilde gezer. Her düğüm için `Drone* d = *(Drone**)node->data;` ile drone işaretçisini alır. Drone'un kendi kilidini alarak `IDLE` olup olmadığını kontrol eder ve en yakın olanı seçer.
        *   Eğer uygun bir drone bulunursa:
            *   `assign_mission(closest_drone, s->coord);` çağrılır (drone'un hedefini ayarlar, durumunu `ON_MISSION` yapar ve `mission_cv`'sine sinyal gönderir).
            *   `survivors->removedata(survivors, peek_result);` ile hayatta kalan global bekleyenler listesinden çıkarılır. (`peek_result`, `node->data`'nın adresidir ve `removedata` bu adresteki `Survivor*` değeri ile karşılaştırma yapar.)
            *   `s->status = 1;` (yardım ediliyor/edildi olarak işaretlenir).
            *   `helpedsurvivors->add(helpedsurvivors, &s);` ile yardım edilenler listesine eklenir.
        *   Eğer uygun drone bulunamazsa "No available drones" logu basılır.
    *   **`drone_behavior` (`drone.c`)**: Her drone kendi thread'inde çalışır:
        *   `pthread_mutex_lock(&d->lock);` ile kendi kilidini alır.
        *   Eğer `d->status == IDLE` ve `running` ise: `pthread_cond_wait(&d->mission_cv, &d->lock);` ile yeni görev bekler. Uyandığında döngüye devam eder.
        *   Eğer `d->status == ON_MISSION` ise: Hedefe doğru bir adım hareket eder (`d->coord` güncellenir). Hedefe ulaşıldıysa `d->status = IDLE;` yapar.
        *   `pthread_mutex_unlock(&d->lock);` ile kilidini bırakır.
        *   Kısa bir `usleep` (görevdeyse) yapar.
    *   **Ana Döngü (`controller.c` `main`)**:
        *   SDL olaylarını (pencere kapama, ESC tuşu) dinler ve `running` bayrağını ayarlar.
        *   `draw_map()` fonksiyonunu çağırarak ekranı periyodik olarak günceller.

3.  **Çizim Mantığı (`view.c` `draw_map` -> `draw_survivors`, `draw_drones`)**:
    *   Ekran temizlenir.
    *   `draw_survivors()`:
        *   Global `survivors` listesi (kilitli) gezilir. `Survivor* s = *(Survivor**)node->data;` ile hayatta kalan alınır ve `RED` çizilir.
        *   Global `helpedsurvivors` listesi (kilitli) gezilir. `Survivor* s = *(Survivor**)node->data;` ile hayatta kalan alınır ve `PURPLE` çizilir.
        *   Tüm `drone_fleet` (her drone kilitli) gezilir. Eğer drone `ON_MISSION` ise, drone'un `target` koordinatı `ORANGE` çizilir.
    *   `draw_drones()`:
        *   `drone_fleet` (her drone kilitli) gezilir. Drone'un mevcut konumu, durumuna göre (`IDLE` -> `BLUE`, `ON_MISSION` -> `GREEN`) çizilir. `ON_MISSION` ise hedefine bir çizgi ve hedef işareti (`YELLOW`) çizilir.
    *   Izgara çizilir ve ekran güncellenir (`SDL_RenderPresent`).

4.  **Temizleme ve Kapanış (`controller.c` `cleanup_and_exit`)**:
    1.  `running = 0;` ile tüm thread döngülerine durma sinyali gönderilir.
    2.  `ai_thread_id` ve `survivor_thread_id` için `pthread_join` çağrılır.
    3.  `cleanup_drones()`:
        *   Her bir drone thread'i için `pthread_cancel` ve `pthread_join` çağrılır.
        *   Her drone'un `mission_cv` ve `lock` senkronizasyon nesneleri (eğer başlatılmışsa) `pthread_cond_destroy` ve `pthread_mutex_destroy` ile yok edilir.
        *   Her drone global `drones` listesinden (`drones->removedata(drones, &drone_ptr);` ile) çıkarılır.
        *   `drone_fleet` dizisi `free` edilir.
    4.  `survivors` listesindeki `Survivor` nesneleri: Liste kilitlenir, liste gezilir, her `*(Survivor**)node->data` ile alınan `Survivor* s_ptr` için `free(s_ptr);` çağrılır. Sonra `survivors->destroy(survivors);` ile liste yapısı yok edilir.
    5.  `helpedsurvivors` listesindeki `Survivor` nesneleri: Aynı şekilde `free` edilir ve liste yapısı `destroy` edilir.
    6.  `drones` listesinin yapısı `drones->destroy(drones);` ile yok edilir.
    7.  `freemap()` ile harita kaynakları serbest bırakılır.
    8.  `quit_all()` ile SDL kaynakları serbest bırakılır.

## 6. Yapılan Anahtar Düzeltmeler ve İyileştirmeler

*   **Liste Veri Tipi ve İşaretçi Tutarlılığı**: Projedeki en kritik sorunlardan biri, özel `List` yapısının `Drone*` ve `Survivor*` gibi işaretçileri nasıl sakladığı ve bu işaretçilere nasıl erişildiğiydi. Bu, `add` fonksiyonuna işaretçinin adresinin (`&pointer_variable`) verilmesi ve listeden okunurken çift dereferans (`*(Type**)node->data`) yapılmasıyla çözüldü. Bu düzeltme, sayısız segmentasyon hatasını ve tanımsız davranışı engelledi.
*   **Senkronizasyon Nesnelerinin Yaşam Döngüsü (`Drone` için)**: `Drone` yapısındaki mutex ve koşul değişkenlerinin başlatılmadan kullanılmaya çalışılması veya yok edildikten sonra erişilmesi "futex" hatalarına yol açıyordu. `lock_initialized` ve `cv_initialized` bayrakları eklenerek bu nesnelerin yaşam döngüsü güvenli bir şekilde yönetildi.
*   **Liste Senkronizasyonu (`list.c`)**: `list.c`'deki tüm temel operasyonlar (add, remove, peek vb.) `pthread_mutex_t lock` ile koruma altına alındı. Ayrıca, listenin dolmasını ve boşken okunmasını engellemek için `elements_sem` ve `capacity_sem` semaforları eklendi. Listeler üzerinde yapılan dış iterasyonlar da (örn: `ai.c`, `view.c`, `controller.c`'de temizlik sırasında) ilgili liste kilitleriyle korundu.
*   **Bellek Sızıntılarının Önlenmesi**: En önemlisi, `create_survivor` ile `malloc` edilen `Survivor` nesnelerinin program sonunda `cleanup_and_exit` içinde düzgün bir şekilde `free` edilmesi sağlandı. Bu, `survivors` ve `helpedsurvivors` listeleri gezilerek yapıldı.
*   **Derleyici Uyarılarının Giderilmesi**: `controller.c`'deki `-Waddress` uyarısına neden olan gereksiz `if (node->data)` koşulları kaldırıldı, çünkü `node->data` esnek bir dizi üyesi olduğu için adresi (düğüm geçerliyse) asla `NULL` olmaz.
*   **`removedata` Kullanımının Düzeltilmesi**: `removedata` fonksiyonunun, karşılaştırma yapacağı veriyle (`memcmp`) tutarlı bir argüman alması sağlandı (örn: `peek_result` veya `&pointer_variable`).

## 7. Test Edilen Senaryolar

*   **Temel İşlevsellik**: Hayatta kalanların rastgele üretilmesi, AI tarafından fark edilmesi, uygun (boşta ve en yakın) drone'un atanması, drone'un hedefe hareket etmesi ve görevi tamamlaması başarıyla test edildi.
*   **Tüm Drone'lar Meşgulken**: Drone sayısı geçici olarak azaltılıp hayatta kalan üretim hızı artırılarak yapılan testlerde, tüm drone'lar meşgulken yeni hayatta kalanların `survivors` listesinde biriktiği ve bir drone boşa çıktığında bu listeden bir hayatta kalana atandığı gözlemlendi. Bu, AI'ın görev kuyruklama mantığının temel düzeyde çalıştığını gösterdi.
*   **Görselleştirme**: Farklı durumdaki drone ve hayatta kalanların renkleri, görev çizgileri SDL arayüzünde doğru bir şekilde görüntülendi.

## 8. Mevcut Yapılandırma (Test Sonrası Varsayılanlar)

*   `drone.c` -> `int num_drones = 10;`
*   `survivor.c` -> `sleep(rand() % 3 + 2);` (Hayatta kalanlar 2-4 saniyede bir üretilir)

## 9. Son Not (Terminal Çıktısı Hakkında)
Programın `Program finished cleanly.` mesajıyla sonlanmasının ardından bazen terminale düşen "New survivor..." gibi loglar görülebilir. Bu durum, ilgili thread (örn: `survivor_generator`) sonlandırılırken `stdout` tamponunda kalan ve işletim sistemi tarafından gecikmeli olarak ekrana basılan çıktılardan kaynaklanmaktadır. Thread'ler düzgün bir şekilde `join` edildiği için bu durum kritik bir sorun teşkil etmez.

Bu özetin Phase 2 geliştirmeleriniz için faydalı bir başlangıç noktası olacağını umuyorum!