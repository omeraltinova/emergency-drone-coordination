# Phase 1 Özeti ve Sistem Mimarisi

Bu doküman, Acil Durum Drone Koordinasyon Simülatörü projesinin Phase 1 aşamasında yapılan geliştirmeleri, düzeltmeleri ve sistemin mevcut mimarisini özetlemektedir. Amaç, Phase 1'in gereksinimlerinin nasıl karşılandığını göstermek ve projenin sonraki aşamaları için bir referans noktası oluşturmaktır.

## 1. Projeye Genel Bakış ve Amaç

Proje, C programlama dili ve POSIX thread'leri (pthreads) kullanılarak geliştirilmiş çoklu iş parçacıklı bir simülasyondur. Temel amaç, bir harita üzerinde rastgele beliren hayatta kalanları (survivors) tespit edip onlara yardım göndermek üzere otonom drone'ları koordine etmektir. Sistem, eş zamanlılık yönetimi, kaynak paylaşımı ve thread güvenliği gibi sistem programlama konseptlerini içermektedir.

## 2. Temel Bileşenler ve Sorumlulukları

Sistem ana olarak aşağıdaki modüllerden oluşmaktadır:

*   **`controller.c`**: Ana kontrolcü. Simülasyonu başlatır, global değişkenleri ve veri yapılarını (drone listesi, hayatta kalan listeleri, harita) ilklendirir. Diğer thread'leri (AI, survivor generator, drone'lar) oluşturur ve yönetir. Simülasyonun sonlandırılmasından ve kaynakların temizlenmesinden sorumludur.
*   **`drone.c` / `headers/drone.h`**: Drone'ların davranış mantığını ve veri yapısını içerir. Her drone ayrı bir thread olarak çalışır. Drone'lar, AI tarafından kendilerine atanan hayatta kalanlara doğru hareket eder, hedefe ulaştıklarında "yardım etmiş" sayılır ve yeni görev beklerler.
    *   **Senkronizasyon**: Her drone, görev atamalarını beklemek için bir koşul değişkenine (`pthread_cond_t`) ve kendi durumunu korumak için bir mutex'e (`pthread_mutex_t`) sahiptir.
    *   **Durumlar**: `IDLE`, `EN_ROUTE`, `ON_MISSION` gibi durumları olabilir.
*   **`survivor.c` / `headers/survivor.h`**: Hayatta kalanların oluşturulması ve yönetilmesinden sorumludur.
    *   `survivor_generator` thread'i, belirli aralıklarla rastgele koordinatlarda yeni hayatta kalanlar oluşturur ve bunları hem global "bekleyen hayatta kalanlar" listesine hem de haritadaki ilgili hücrenin listesine ekler.
*   **`ai.c` / `headers/ai.h`**: Yapay zeka (AI) kontrolcüsü. Ayrı bir thread olarak çalışır.
    *   Global "bekleyen hayatta kalanlar" listesini sürekli kontrol eder.
    *   Yeni bir hayatta kalan tespit edildiğinde, en yakın ve `IDLE` durumdaki drone'u bulur.
    *   Drone'a görevi atar (hedef koordinatları ve hayatta kalanın bilgisi), drone'un koşul değişkenini sinyalleyerek onu uyandırır ve hayatta kalanı "bekleyenler" listesinden çıkarıp "yardım edilenler" listesine taşır (mantıksal olarak).
*   **`map.c` / `headers/map.h`**: Simülasyon haritasını yönetir. Harita, hücrelerden oluşan bir grid'dir. Her hücre, kendi içinde o an o hücrede bulunan hayatta kalanların bir listesini tutabilir. Bu, Phase 1'de tam olarak kullanılmasa da, gelecekteki özellikler için bir altyapı sunar.
*   **`view.c` / `headers/view.h`**: Simülasyonun metin tabanlı (ncurses) görsel arayüzünü yönetir. Haritayı, drone'ları, hayatta kalanları (bekleyen ve yardım edilmiş) farklı renklerle gösterir. Ayrı bir thread'de çalışarak simülasyon durumunu periyodik olarak günceller.
*   **`list.c` / `headers/list.h`**: Thread-güvenli (thread-safe) genel amaçlı bir bağlı liste veri yapısıdır. Projedeki tüm dinamik listeler (drone'lar, hayatta kalanlar) bu yapıyı kullanır.
    *   **Özellikler**: Ekleme, silme, arama, başa/sona ekleme, peek (ilk elemana bakma) gibi temel liste operasyonlarını destekler.
    *   **Thread Güvenliği**: Her liste örneği, operasyonlar sırasında veri bütünlüğünü korumak için kendi mutex'ine sahiptir. Bazı operasyonlar (örn: `add`, `removedata`) listenin dolu/boş olma durumunu yönetmek için semafor da kullanabilir, ancak mevcut implementasyonda semaforlar `MAX_SIZE` kontrolü için kullanılıyor.

## 3. Anahtar Veri Yapıları ve Yönetimi

### 3.1. Genel Amaçlı Thread-Güvenli Liste (`List`)

*   **Tanım (`headers/list.h`):**
    *   `Node`: Liste düğümü. `void *data` (herhangi bir tipte veri saklayabilir), `Node *next`, `Node *prev`.
    *   `List`: Liste başlığı. `Node *head`, `Node *tail`, `int size`, `int max_size`, `size_t data_size` (saklanan verinin boyutu), `pthread_mutex_t lock`, `sem_t sem_full`, `sem_t sem_empty`, ve çeşitli fonksiyon işaretçileri (`add`, `removedata`, `peek`, `destroy` vb.).
*   **Önemli Düzeltmeler ve Notlar:**
    *   **Veri Saklama Yöntemi:** Liste, `memcpy` kullanarak verilerin *kopyalarını* saklar. Bu nedenle, listeye eklenen verinin bir işaretçi olması durumunda, işaretçinin kendisi değil, işaret ettiği *değerin adresi* listeye eklenmelidir. Örneğin, `Survivor* s_ptr` varsa ve liste `Survivor*` saklayacaksa, `list->add(list, &s_ptr)` şeklinde ekleme yapılır. Listeden okuma yapılırken de `Survivor* okunan_s_ptr = *(Survivor**)node->data;` şeklinde çift dolaylı yönlendirme (double dereference) gerekir.
    *   Bu proje boyunca `Drone*`, `Survivor*` gibi işaretçilerin listelerde saklanması için bu yöntem benimsenmiştir.
    *   `removenode` içindeki `list->lastprocessed` ataması, `find_memcell_fornode` ile potansiyel sorunlar yaratabileceği için yorum satırı yapılmıştır.
    *   `printlist`, `printlistfromtail`, `peek` gibi okuma fonksiyonlarına mutex koruması eklenmiştir.
    *   `destroy` fonksiyonu, liste tarafından kullanılan mutex ve semaforları düzgün bir şekilde yok eder.

### 3.2. Global Listeler (`controller.c`)

*   `List *drones`: Global drone listesi. `Drone*` işaretçileri saklar.
*   `List *survivors`: Global "bekleyen hayatta kalanlar" listesi. `Survivor*` işaretçileri saklar.
*   `List *helpedsurvivors`: Global "yardım edilmiş hayatta kalanlar" listesi. `Survivor*` işaretçileri saklar.
    *   Bu listelerin tümü `create_list(sizeof(PointerType*), ...)` şeklinde oluşturulmuştur (örneğin, `create_list(sizeof(Drone*), ...)`).

## 4. Eş Zamanlılık ve Senkronizasyon

*   **Thread'ler:**
    *   Ana thread (`controller.c`)
    *   AI controller thread'i (`ai.c`)
    *   Survivor generator thread'i (`survivor.c`)
    *   Her drone için bir thread (`drone.c`)
    *   View (görsel arayüz) thread'i (`view.c`)
*   **Senkronizasyon Araçları:**
    *   **Mutex'ler (`pthread_mutex_t`):**
        *   Her `List` örneği kendi erişimini korumak için bir mutex'e sahiptir.
        *   Her `Drone` kendi iç durumunu (state, target, vs.) ve koşul değişkenini korumak için bir mutex'e sahiptir.
        *   Görsel arayüz (ncurses) operasyonları da global bir mutex (`ncurses_lock`) ile korunur.
    *   **Koşul Değişkenleri (`pthread_cond_t`):**
        *   Her `Drone`, `IDLE` durumundayken yeni bir görev atanmasını beklemek için bir koşul değişkeni kullanır. AI, bir görev atadığında bu koşul değişkenini sinyaller.
*   **Önemli Senkronizasyon Düzeltmeleri:**
    *   **Futex Hataları:** Başlangıçta karşılaşılan "futex facility returned an unexpected error code" hatası, genellikle senkronizasyon nesnelerinin (mutex, CV) başlatılmadan kullanılması veya yok edildikten sonra erişilmesiyle ilgilidir.
        *   **Çözüm:** `Drone` yapısına `lock_initialized` ve `cv_initialized` bayrakları eklendi. `initialize_drones` fonksiyonu bu bayrakları mutex/CV başarıyla başlatılırsa `true` yapar. `cleanup_drones` ise sadece bu bayraklar `true` ise ilgili senkronizasyon nesnesini yok eder. Bu, kaynakların güvenli bir şekilde yönetilmesini sağlar.
    *   Liste operasyonlarının (özellikle `peek`, `printlist`) mutex koruması eksikti ve eklendi.

## 5. Phase 1 Boyunca Yapılan Ana Düzeltmeler ve İyileştirmeler

1.  **Liste Veri Tipi Tutarsızlıkları Giderildi:**
    *   `map.c` (`init_map`), `controller.c` (`main`), `drone.c` (`initialize_drones`), `ai.c` (`find_closest_idle_drone`) dosyalarındaki liste kullanımları, işaretçilerin doğru şekilde (`Type*` yerine `Type**` mantığıyla) saklanması ve okunması için düzeltildi. Bu, `add` ve `removedata` çağrılarının ve liste elemanlarına erişimin güncellenmesini içeriyordu.

2.  **`list.c` Fonksiyonlarında İyileştirmeler:**
    *   Mutex korumaları eklendi/geliştirildi.
    *   `removenode` içindeki potansiyel sorunlu satır yorumlandı.
    *   `destroy` fonksiyonunun kaynakları temizlediğinden emin olundu.

3.  **`survivor.c` Düzeltmeleri:**
    *   `survivor_cleanup` içindeki gereksiz mutex kilitleri kaldırıldı.
    *   `survivor_generator`da, bir hayatta kalanın harita hücresine eklenememesi durumunda global listeden de çıkarılması ve belleğinin serbest bırakılması sağlandı.

4.  **`drone.c` Senkronizasyon ve Kaynak Yönetimi Düzeltmeleri:**
    *   `cleanup_drones` fonksiyonunda, drone'ların global `drones` listesinden çıkarılması eklendi.
    *   Futex hatalarını önlemek için `lock_initialized` ve `cv_initialized` bayrakları ile güvenli mutex/CV başlatma ve yok etme mekanizması eklendi.

5.  **`controller.c` Temizleme İşlemleri:**
    *   `cleanup_and_exit` fonksiyonunda `cleanup_drones()` çağrısının doğru yerde yapılması sağlandı.
    *   **Bellek Sızıntısı Giderimi:** Simülasyon sonunda, `survivors` ve `helpedsurvivors` listelerinde kalan `Survivor` nesnelerinin bellekten `free()` ile serbest bırakılması eklendi. Bu, `Lab for Phase-1.md` dosyasında belirtilen bir gereksinimdi.

6.  **Header Dosyası ve Derleme Hataları:**
    *   `headers/drone.h` dosyasına `<stdbool.h>` eklendi ve `Drone` yapısına yeni bayraklar ( `lock_initialized`, `cv_initialized`) dahil edildi. Bu, derleme hatalarını çözdü.

7.  **`view.c` İşaretçi Düzeltmeleri:**
    *   `draw_survivors` fonksiyonunda, listelerden `Survivor*` okunurken doğru şekilde çift dolaylı yönlendirme (`*(Survivor**)node->data`) kullanılması sağlandı. Bu, kurtarılan hayatta kalanların haritada doğru renkte (yeşil) gösterilmesini sağladı.

8.  **Derleyici Uyarılarının Giderilmesi:**
    *   `controller.c` dosyasındaki `cleanup_and_exit` fonksiyonunda, `if (current_survivor_node->data)` gibi her zaman doğru olan gereksiz koşullar `-Waddress` uyarısına neden oluyordu. Bu koşullar kaldırıldı.

## 6. Mevcut Durum ve Simülasyonun Çalıştırılması

*   Kod şu anda `make` komutu ile derlenebilir durumdadır.
*   `./drone_simulator` komutu ile çalıştırılabilir.
*   10 drone ve rastgele (2-5 saniyede bir) beliren hayatta kalanlar varsayılan ayarlardır.
*   AI, bekleyen hayatta kalanları en yakın boşta drone'a atar.
*   Drone'lar görevlerini tamamladıklarında yeni görev bekler.
*   Görsel arayüz, durumu ncurses ile gösterir.
*   Program sonlandığında (Ctrl+C ile), tüm thread'ler düzgün bir şekilde sonlandırılır ve tüm dinamik bellek (drone'lar, hayatta kalanlar, listeler) serbest bırakılır.

## 7. Gözlemlenen Davranışlar ve Testler

*   **Tüm Drone'lar Meşgulken Hayatta Kalan Üretimi:** Drone sayısı geçici olarak 2'ye düşürülüp hayatta kalan üretim frekansı artırılarak test edildi. Bu senaryoda, yeni hayatta kalanlar `survivors` listesinde birikir ve bir drone boşa çıktığında AI tarafından atanır. Bu, sistemin yoğun yük altında doğru çalıştığını gösterir.
*   **Hayatta Kalan Atama Stratejisi:** AI, mevcut implementasyonda hayatta kalanlara en yakın `IDLE` drone'u atar (`find_closest_idle_drone`). Bu, `Lab for Phase-1.md` dosyasında belirtilen "en yakın drone" stratejisine uygundur.

Bu özet, Phase 1'deki ana çalışmaları ve sistemin mevcut durumunu kapsamaktadır. Phase 2 ve ötesi için iyi bir başlangıç noktası sunmalıdır. 