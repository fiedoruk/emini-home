/* Served by Home itself. Every preview and mutation uses the same-origin API. */
(() => {
  "use strict";
  const C = HomeCore,
    $ = (s) => document.querySelector(s),
    app = $("#app");
  const read = (k) => {
    try {
      return localStorage.getItem(k);
    } catch {
      return null;
    }
  };
  const write = (k, v) => {
    try {
      v === null ? localStorage.removeItem(k) : localStorage.setItem(k, v);
    } catch {
      /* In-memory session still works. */
    }
  };
  const S = {
    token: read("home.token") || "",
    lang: read("home.language") || "en",
    theme: read("home.theme") || "light",
    config: null,
    draft: null,
    status: null,
    tab: "screens",
    selected: "weather",
    dirty: false,
    online: false,
    mutation: false,
    polling: false,
    frames: {},
    draftFrames: {},
    frame: null,
    frameGeneration: null,
    conflict: false,
    noticeTimer: null,
    previewTimer: null,
    previewEpoch: 0,
    previewController: null,
  };
  if (!["en", "pl"].includes(S.lang)) S.lang = "en";
  if (!["light", "dark"].includes(S.theme)) S.theme = "light";
  const text = {
    en: {
      footerLine: "A little more room for your day.",
      footerLink: "emini Home website ↗",
      footerPrivacy:
        "Your settings stay on your Home. No account. Home sends no analytics.",
      connecting: "Connecting to Home…",
      connected: "Connected directly to your Home",
      offline: "Home is out of reach. Check your Wi-Fi.",
      retry: "Retry",
      pairEyebrow: "YOUR PHONE + YOUR HOME",
      pairTitle: "Make yourself at Home.",
      pairIntro:
        "A quiet place for your weather, your people and a few words that matter.",
      pairHelp:
        "Enter the six-digit code shown on your Home. Your phone will remember this connection.",
      code: "Code on your Home",
      pair: "Connect this phone",
      pairNote:
        "Wait for the image to finish refreshing, then hold OK / BOOT for 2 seconds. Pairing and the setup Wi-Fi stay open for 5 minutes. Use a trusted network; this local connection uses HTTP.",
      pairError: "That code did not work. Check the screen and try again.",
      pairLimit:
        "This pairing window has closed. Codes expire after 5 minutes or the attempt limit. Wait for the image to finish, then hold OK / BOOT for 2 seconds to open pairing again.",
      expired: "Your connection has expired. Pair this phone again.",
      opening: "Opening your Home…",
      title: "Your day, on display.",
      subtitle: "Choose what stays in view. Let the rest wait.",
      eyebrow: "YOUR HOME / LOCAL PANEL",
      local: "PHONE ↔ HOME",
      screens: "Screens",
      rhythm: "Rhythm",
      settings: "Settings",
      share: "Share",
      onScreen: "On your Home",
      confirmed: "Last confirmed image",
      unconfirmed: "No confirmed image yet",
      frameUnknown: "The last confirmed image could not be loaded.",
      frameHint: "Four pigments. One complete picture.",
      photoHint: "Colours are an approximation.",
      ready: "Ready",
      preparing: "Preparing the next image",
      refreshing: "Refreshing the display",
      error: "Display needs attention",
      refreshHint: "A full refresh takes about 25 seconds.",
      pendingHint:
        "A change is requested. This is still the last confirmed image.",
      yourScreens: "Your screens",
      threeScreens: "3 kinds of information",
      weather: "Weather",
      feed: "News",
      note: "Your note",
      sky: "Sky",
      air: "Air",
      weatherDesc: "The pulse of your place",
      feedDesc: "News from around the world",
      noteDesc: "A few words of your own",
      skyDesc: "Sun and moon over your place",
      airDesc: "Air quality, UV and pollen",
      skySource:
        "Worked out on the device from the place you saved; nothing is downloaded.",
      airSource:
        "Air quality, UV and pollen come from Open-Meteo, licensed under CC BY 4.0.",
      airMain: "The big number",
      brush: "Brush",
      brushChoice: "Tone structure",
      brushGrain: "Grain",
      brushHalftone: "Halftone dots",
      brushGrid: "Grid (classic)",
      brushHelp:
        "Four pigments and no grey: every tone is a pattern. Pick the one that paints it: soft grain, printed dots, or the ordered grid of earlier versions.",
      airEU: "European air quality index",
      airUS: "US AQI",
      airPM: "PM2.5 in micrograms per cubic metre",
      edit: "Edit",
      enabled: "Include this screen",
      off: "Not in rotation",
      active: "On the display",
      next: "Selected for editing",
      editTitle: "Make it yours",
      layout: "Composition",
      print: "Print",
      rhythmStyle: "Rhythm",
      atlas: "Atlas",
      cycleStyle: "In turn",
      earlier: "Move earlier",
      later: "Move later",
      savedPreview: "Preview of saved settings",
      draftPreview: "Save your changes to update this preview.",
      previewMissing: "Preview is unavailable. Save settings and try again.",
      location: "Place name",
      latitude: "Latitude",
      longitude: "Longitude",
      coordsHelp:
        "Use the coordinates of a town, rather than your exact home address.",
      noteText: "Your words",
      noteHelp: "A short thought works best. Watch how much space it takes.",
      feedURL: "Public RSS / Atom address",
      feedHelp: "Use an HTTPS feed. Leave this empty to turn the source off.",
      source: "Source",
      fresh: "Current",
      stale: "Older data",
      empty: "No data yet",
      unknown: "Time not confirmed",
      fetching: "Checking for updates",
      disabled: "Not configured",
      failed: "Could not update",
      issued: "Information from",
      fetched: "Last downloaded",
      checked: "Last checked",
      expires: "Check again after",
      never: "Not yet",
      refreshSource: "Check for updates",
      sourceQueued: "Update requested. Home respects the source’s cache.",
      byteCount: "bytes",
      saved: "Saved on Home",
      unsaved: "Unsaved changes",
      save: "Save settings",
      saving: "Saving…",
      show: "Show now",
      showHint: "Saved settings only",
      showAccepted: "Image requested. Wait for Home to finish refreshing.",
      saveDone: "Settings saved. The display has not been changed.",
      validation: "Please check these fields:",
      conflict:
        "Settings changed on Home. Your draft is still here. Reload the saved settings before continuing.",
      reload: "Reload saved settings",
      reloadConfirm:
        "Discard this draft and load the latest settings from Home?",
      requestFailed: "Home could not complete this request. Try again.",
      busy: "Home is busy. Please try again shortly.",
      rhythmTitle: "A pace that feels right.",
      rhythmIntro:
        "Check for new information without chasing it. Choose when the picture changes.",
      mode: "How screens change",
      fixed: "One screen",
      day: "Day rhythm",
      rotate: "Rotation",
      fixedScreen: "Keep this screen",
      interval: "Minutes per screen",
      intervalHelp: "5–1440 minutes. Home combines changes during a refresh.",
      cycleInterval: "Change composition every … minutes",
      cycleIntervalHelp:
        "5–1440 minutes, while this screen stays on the display.",
      dayHelp:
        "Three moments, in time order. Home follows your saved time zone.",
      time: "Time",
      screen: "Screen",
      days: "Active days",
      dayNames: ["M", "T", "W", "T", "F", "S", "S"],
      dayFull: [
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday",
      ],
      quiet: "Quiet hours",
      quietEnable: "Keep the last picture at night",
      start: "From",
      end: "Until",
      quietHelp:
        "Home keeps the last image during quiet hours. This is not a sleep or battery setting.",
      pause: "Take a pause",
      pauseMinutes: "Pause after a physical button press",
      pauseHelp:
        "Manual choices come first. Use 0 to allow automation immediately.",
      pauseNow: "Pause for 60 min",
      resume: "Resume rhythm",
      paused: "Automatic changes paused",
      resumed: "Automatic changes resumed",
      timeMissing:
        "Home has not confirmed the time. Scheduled changes wait until its clock is ready.",
      settingsTitle: "Settle into your Home.",
      settingsIntro:
        "Your appearance, preferences and connection. All stored on this device.",
      appearance: "A little more character",
      texture: "Pixel texture",
      intensity: "Colour pattern",
      soft: "Black & white",
      balanced: "Balanced",
      bold: "Expressive",
      largeText: "Larger text",
      profile: "Start with a profile",
      desk: "Desk",
      distance: "At a glance",
      showcase: "Showcase",
      profileHelp:
        "Desk: day rhythm. At a glance: larger text and a fixed screen. Showcase: bold public screens rotating every 30 minutes; your note stays out.",
      profileApplied: "Profile applied to your draft. Review it, then save.",
      preferences: "Your preferences",
      name: "Name of your Home",
      language: "Display language",
      okAction: "Short press of OK / BOOT",
      okLanguage: "Switch the display language",
      okRefresh: "Check for updates now",
      okHold: "Hold the current screen (press again to resume)",
      okSetup: "Open the setup window (Wi-Fi and pairing)",
      okHelp: "Holding OK / BOOT for 2 seconds always opens the setup window.",
      units: "Temperature",
      timezone: "Time zone",
      clock: "24-hour clock",
      device: "Your device",
      version: "Software",
      address: "Local address",
      generation: "Completed pictures",
      wifi: "Home Wi-Fi",
      wifiIntro:
        "Connect Home to your Wi-Fi so it can fetch information on its own. Your password is sent only to this device and is not kept in this page.",
      ssid: "Network name",
      password: "Wi-Fi password",
      wifiConnect: "Connect to Wi-Fi",
      wifiQueued:
        "Wi-Fi setup requested. This connection may close; reconnect your phone to the same home network.",
      forget: "Disconnect this phone",
      forgetConfirm: "Remove this phone’s access to Home?",
      forgot: "Phone disconnected.",
      shareTitle: "Pass the idea around.",
      shareIntro:
        "Let someone else make your composition their own. Your personal information stays with you.",
      recipeEyebrow: "MADE TO BE REMIXED",
      recipeTitle: "Your design. Their day.",
      recipeCopy:
        "A recipe carries the layout and rhythm. It leaves behind your location, note, source addresses and connection details.",
      exportRecipe: "Download your recipe",
      importTitle: "Bring a recipe home",
      importCopy:
        "Choose a Home recipe file. Review what it changes before importing.",
      chooseRecipe: "Choose a recipe file",
      recipeReview: "Review this recipe",
      recipeReviewCopy:
        "This changes appearance and rhythm on Home. Your place, words and sources are kept. It does not change the display immediately.",
      importRecipe: "Import recipe",
      imported: "Recipe saved. You can now choose a screen to show.",
      cancel: "Cancel",
      imageTitle: "Show the whole picture",
      imageCopy:
        "Start with a public sample, or deliberately choose the last confirmed image from your device. Review before downloading.",
      reviewImage: "Prepare an image",
      imageReview: "Choose what to share",
      publicSample: "Public sample",
      personalImage: "My displayed image",
      imagePrivacy:
        "Your displayed image may contain your location, personal words or setup codes and Wi-Fi details. Nothing is uploaded. Check every part before downloading.",
      downloadImage: "Download this image",
      sampleLabel: "Public sample · not your device data",
      sampleTitle: "A little room",
      sampleTitle2: "for your day.",
      sampleSub: "FOUR COLOURS · NO RUSH",
      recipeError: "This is not a complete, supported Home recipe.",
      recipeSize: "Choose a recipe smaller than 16 KB.",
      errorFields: "Please correct the highlighted settings.",
      refreshUnavailable: "No preview is available yet.",
      wifiEmpty: "Enter your Wi-Fi network name.",
      savingBlocked: "Save or discard your current draft first.",
      notEnabled: "Enable this screen before showing it.",
      reloading: "Reloading saved settings…",
      statusUnknown: "Waiting for Home’s status",
      sample: "SAMPLE",
      palette: "Four-colour preview",
      theme: "Change colour theme",
      changeLanguage: "Zmień język na polski",
      dismiss: "Close",
      importSummary: "Settings in this recipe",
      noPrivate:
        "No location, note, feed address or connection details are included.",
      privateRemoved:
        "Private or unknown fields were removed from this recipe.",
      offlineDraft: "Disconnected · your draft stays on this page",
      pending: "Change requested",
      duration: "Last refresh",
      refreshSeconds: "s",
      deviceStatus: "Display status",
    },
    pl: {
      footerLine: "Trochę więcej miejsca na Twój dzień.",
      footerLink: "Strona emini Home ↗",
      footerPrivacy:
        "Ustawienia zostają na Twoim Home. Bez konta. Home nie wysyła analityki.",
      connecting: "Łączenie z Home…",
      connected: "Połączenie bezpośrednio z Twoim Home",
      offline: "Home poza zasięgiem. Sprawdź Wi-Fi.",
      retry: "Ponów",
      pairEyebrow: "TWÓJ TELEFON + TWÓJ HOME",
      pairTitle: "Rozgość się w Home.",
      pairIntro:
        "Spokojne miejsce na pogodę, Twoich ludzi i kilka ważnych słów.",
      pairHelp:
        "Wpisz sześciocyfrowy kod widoczny na Home. Telefon zapamięta to połączenie.",
      code: "Kod na ekranie Home",
      pair: "Połącz ten telefon",
      pairNote:
        "Poczekaj na koniec odświeżania obrazu, potem przytrzymaj OK / BOOT przez 2 sekundy. Parowanie i sieć konfiguracyjna otworzą się na 5 minut. Korzystaj z zaufanej sieci; to lokalne połączenie używa HTTP.",
      pairError: "Kod nie zadziałał. Sprawdź ekran i spróbuj ponownie.",
      pairLimit:
        "To okno parowania jest zamknięte. Kod wygasa po 5 minutach lub po wyczerpaniu prób. Poczekaj na koniec odświeżania i przytrzymaj OK / BOOT przez 2 sekundy, aby otworzyć je ponownie.",
      expired: "Dostęp tego telefonu wygasł. Sparuj go ponownie.",
      opening: "Otwieranie Twojego Home…",
      title: "Twój dzień na ekranie.",
      subtitle: "Wybierz, co zostaje w zasięgu wzroku. Reszta może poczekać.",
      eyebrow: "TWÓJ HOME / PANEL LOKALNY",
      local: "TELEFON ↔ HOME",
      screens: "Ekrany",
      rhythm: "Rytm",
      settings: "Ustawienia",
      share: "Podziel się",
      onScreen: "Na Twoim Home",
      confirmed: "Ostatni potwierdzony obraz",
      unconfirmed: "Brak potwierdzonego obrazu",
      frameUnknown: "Nie udało się wczytać ostatniego potwierdzonego obrazu.",
      frameHint: "Cztery pigmenty. Jeden pełny obraz.",
      photoHint: "Kolory są przybliżeniem.",
      ready: "Gotowy",
      preparing: "Przygotowanie kolejnego obrazu",
      refreshing: "Odświeżanie ekranu",
      error: "Ekran wymaga uwagi",
      refreshHint: "Pełne odświeżenie trwa około 25 sekund.",
      pendingHint:
        "Zmiana została zlecona. To nadal ostatni potwierdzony obraz.",
      yourScreens: "Twoje ekrany",
      threeScreens: "3 rodzaje informacji",
      weather: "Pogoda",
      feed: "Wiadomości",
      note: "Twoja kartka",
      sky: "Niebo",
      air: "Powietrze",
      weatherDesc: "Puls Twojego miejsca",
      feedDesc: "Wiadomości ze świata",
      noteDesc: "Kilka własnych słów",
      skyDesc: "Słońce i księżyc nad Twoim miejscem",
      airDesc: "Jakość powietrza, UV i pyłki",
      skySource:
        "Liczone na urządzeniu z zapisanej lokalizacji; nic nie jest pobierane.",
      airSource:
        "Jakość powietrza, UV i pyłki pochodzą z Open-Meteo, na licencji CC BY 4.0.",
      airMain: "Duża liczba",
      brush: "Pędzel",
      brushChoice: "Struktura tonów",
      brushGrain: "Ziarno",
      brushHalftone: "Kropka",
      brushGrid: "Siatka (klasyczna)",
      brushHelp:
        "Cztery pigmenty i żadnej szarości: każdy ton to wzór. Wybierz ten, który go maluje: miękkie ziarno, kropkę jak w druku albo klasyczną siatkę z wcześniejszych wersji.",
      airEU: "Europejski indeks jakości powietrza",
      airUS: "Indeks US AQI",
      airPM: "PM2,5 w mikrogramach na metr sześcienny",
      edit: "Edytuj",
      enabled: "Uwzględnij ten ekran",
      off: "Poza rotacją",
      active: "Na urządzeniu",
      next: "Wybrany do edycji",
      editTitle: "Po Twojemu",
      layout: "Kompozycja",
      print: "Plakat",
      rhythmStyle: "Rytm",
      atlas: "Atlas",
      cycleStyle: "Po kolei",
      earlier: "Przesuń wcześniej",
      later: "Przesuń później",
      savedPreview: "Podgląd zapisanych ustawień",
      draftPreview: "Zapisz zmiany, aby zaktualizować ten podgląd.",
      previewMissing:
        "Podgląd niedostępny. Zapisz ustawienia i spróbuj ponownie.",
      location: "Nazwa miejsca",
      latitude: "Szerokość geograficzna",
      longitude: "Długość geograficzna",
      coordsHelp:
        "Użyj współrzędnych miejscowości, zamiast dokładnego adresu domu.",
      noteText: "Twoje słowa",
      noteHelp:
        "Krótka myśl sprawdzi się najlepiej. Sprawdź, ile miejsca zajmuje.",
      feedURL: "Publiczny adres RSS / Atom",
      feedHelp: "Użyj kanału HTTPS. Pozostaw puste, aby wyłączyć źródło.",
      source: "Źródło",
      fresh: "Aktualne",
      stale: "Starsze dane",
      empty: "Jeszcze bez danych",
      unknown: "Czas niepotwierdzony",
      fetching: "Sprawdzanie aktualizacji",
      disabled: "Nieskonfigurowane",
      failed: "Nie udało się odświeżyć",
      issued: "Informacja z",
      fetched: "Ostatnio pobrano",
      checked: "Ostatnio sprawdzono",
      expires: "Kolejna kontrola po",
      never: "Jeszcze nie",
      refreshSource: "Sprawdź aktualizacje",
      sourceQueued:
        "Zlecono sprawdzenie. Home respektuje pamięć podręczną źródła.",
      byteCount: "bajtów",
      saved: "Zapisane na Home",
      unsaved: "Niezapisane zmiany",
      save: "Zapisz ustawienia",
      saving: "Zapisywanie…",
      show: "Pokaż teraz",
      showHint: "Tylko zapisane ustawienia",
      showAccepted: "Zlecono obraz. Zaczekaj na zakończenie odświeżania Home.",
      saveDone:
        "Ustawienia zapisane. Obraz na urządzeniu nie został zmieniony.",
      validation: "Sprawdź te pola:",
      conflict:
        "Ustawienia zmieniły się na Home. Twój szkic zostaje tutaj. Przed dalszą pracą wczytaj zapisane ustawienia.",
      reload: "Wczytaj zapisane ustawienia",
      reloadConfirm:
        "Odrzucić ten szkic i wczytać najnowsze ustawienia z Home?",
      requestFailed: "Home nie wykonał tej operacji. Spróbuj ponownie.",
      busy: "Home jest zajęty. Spróbuj za chwilę.",
      rhythmTitle: "W Twoim tempie.",
      rhythmIntro:
        "Sprawdzaj informacje bez biegania za nimi. Wybierz, kiedy zmienia się obraz.",
      mode: "Jak zmieniają się ekrany",
      fixed: "Jeden ekran",
      day: "Rytm dnia",
      rotate: "Rotacja",
      fixedScreen: "Zachowaj ten ekran",
      interval: "Minuty na ekran",
      intervalHelp: "5–1440 minut. Home scala zmiany w trakcie odświeżania.",
      cycleInterval: "Zmieniaj kompozycję co … minut",
      cycleIntervalHelp: "5–1440 minut, gdy ten ekran zostaje na wyświetlaczu.",
      dayHelp:
        "Trzy chwile, w kolejności godzin. Home używa zapisanej strefy czasu.",
      time: "Godzina",
      screen: "Ekran",
      days: "Aktywne dni",
      dayNames: ["P", "W", "Ś", "C", "P", "S", "N"],
      dayFull: [
        "Poniedziałek",
        "Wtorek",
        "Środa",
        "Czwartek",
        "Piątek",
        "Sobota",
        "Niedziela",
      ],
      quiet: "Godziny ciszy",
      quietEnable: "Zachowaj ostatni obraz nocą",
      start: "Od",
      end: "Do",
      quietHelp:
        "W godzinach ciszy Home zostawia ostatni obraz. To nie jest ustawienie uśpienia ani baterii.",
      pause: "Chwila przerwy",
      pauseMinutes: "Pauza po naciśnięciu przycisku",
      pauseHelp:
        "Ręczny wybór ma pierwszeństwo. Wpisz 0, by od razu dopuścić automat.",
      pauseNow: "Pauza na 60 min",
      resume: "Wznów rytm",
      paused: "Automatyczne zmiany wstrzymane",
      resumed: "Automatyczne zmiany wznowione",
      timeMissing:
        "Home nie potwierdził czasu. Harmonogram poczeka na gotowy zegar.",
      settingsTitle: "Urządź swój Home.",
      settingsIntro:
        "Wygląd, preferencje i połączenie. Wszystko zapisane na tym urządzeniu.",
      appearance: "Trochę więcej charakteru",
      texture: "Faktura pikseli",
      intensity: "Wzór koloru",
      soft: "Czarno-biały",
      balanced: "Zrównoważony",
      bold: "Wyrazisty",
      largeText: "Większy tekst",
      profile: "Zacznij od profilu",
      desk: "Biurko",
      distance: "Z dystansu",
      showcase: "Showcase",
      profileHelp:
        "Biurko: rytm dnia. Z dystansu: większy tekst i stały ekran. Showcase: wyraziste publiczne ekrany co 30 minut, bez własnej kartki.",
      profileApplied: "Profil zastosowano w szkicu. Przejrzyj go i zapisz.",
      preferences: "Twoje preferencje",
      name: "Nazwa Twojego Home",
      language: "Język ekranu",
      okAction: "Krótkie naciśnięcie OK / BOOT",
      okLanguage: "Przełącz język ekranu",
      okRefresh: "Sprawdź aktualizacje",
      okHold: "Zatrzymaj bieżący ekran (drugie naciśnięcie wznawia)",
      okSetup: "Otwórz okno konfiguracji (Wi-Fi i parowanie)",
      okHelp:
        "Przytrzymanie OK / BOOT przez 2 sekundy zawsze otwiera okno konfiguracji.",
      units: "Temperatura",
      timezone: "Strefa czasu",
      clock: "Zegar 24-godzinny",
      device: "Twoje urządzenie",
      version: "Oprogramowanie",
      address: "Adres lokalny",
      generation: "Ukończone obrazy",
      wifi: "Domowe Wi-Fi",
      wifiIntro:
        "Połącz Home z Wi-Fi, aby sam pobierał informacje. Hasło trafia tylko do urządzenia i nie jest zapamiętywane na tej stronie.",
      ssid: "Nazwa sieci",
      password: "Hasło Wi-Fi",
      wifiConnect: "Połącz z Wi-Fi",
      wifiQueued:
        "Zlecono konfigurację Wi-Fi. To połączenie może się zamknąć; połącz telefon z tą samą siecią domową.",
      forget: "Odłącz ten telefon",
      forgetConfirm: "Usunąć dostęp tego telefonu do Home?",
      forgot: "Telefon odłączony.",
      shareTitle: "Podaj pomysł dalej.",
      shareIntro:
        "Niech ktoś nada Twojej kompozycji własną treść. Prywatne informacje zostają u Ciebie.",
      recipeEyebrow: "DO WŁASNEJ INTERPRETACJI",
      recipeTitle: "Twój wzór. Czyjś dzień.",
      recipeCopy:
        "Przepis przenosi kompozycję i rytm. Zostawia Twoje miejsce, notatkę, adresy źródeł i dane połączenia.",
      exportRecipe: "Pobierz swój przepis",
      importTitle: "Zaproś nowy wzór",
      importCopy:
        "Wybierz plik przepisu Home. Przed importem sprawdź, co zmieni.",
      chooseRecipe: "Wybierz plik przepisu",
      recipeReview: "Przejrzyj przepis",
      recipeReviewCopy:
        "Zmieni wygląd i rytm na Home. Zachowa Twoje miejsce, słowa i źródła. Nie zmieni od razu obrazu na ekranie.",
      importRecipe: "Importuj przepis",
      imported: "Przepis zapisany. Możesz wybrać ekran do pokazania.",
      cancel: "Anuluj",
      imageTitle: "Pokaż cały obraz",
      imageCopy:
        "Zacznij od publicznego przykładu lub świadomie wybierz ostatni potwierdzony obraz urządzenia. Obejrzyj go przed pobraniem.",
      reviewImage: "Przygotuj obraz",
      imageReview: "Wybierz, czym się dzielisz",
      publicSample: "Publiczny przykład",
      personalImage: "Mój obraz z urządzenia",
      imagePrivacy:
        "Obraz może zawierać Twoje miejsce, osobiste słowa lub kody parowania i dane Wi-Fi. Niczego nie wysyłamy. Sprawdź całość przed pobraniem.",
      downloadImage: "Pobierz ten obraz",
      sampleLabel: "Publiczny przykład · bez danych urządzenia",
      sampleTitle: "Miejsce na",
      sampleTitle2: "Twój dzień.",
      sampleSub: "CZTERY KOLORY · BEZ POŚPIECHU",
      recipeError: "To nie jest kompletny przepis Home w obsługiwanej wersji.",
      recipeSize: "Wybierz przepis mniejszy niż 16 KB.",
      errorFields: "Popraw wskazane ustawienia.",
      refreshUnavailable: "Podgląd nie jest jeszcze dostępny.",
      wifiEmpty: "Wpisz nazwę swojej sieci Wi-Fi.",
      savingBlocked: "Najpierw zapisz lub odrzuć bieżący szkic.",
      notEnabled: "Włącz ten ekran przed pokazaniem.",
      reloading: "Wczytywanie zapisanych ustawień…",
      statusUnknown: "Oczekiwanie na stan Home",
      sample: "PRZYKŁAD",
      palette: "Podgląd czterech kolorów",
      theme: "Zmień motyw kolorów",
      changeLanguage: "Change language to English",
      dismiss: "Zamknij",
      importSummary: "Ustawienia tego przepisu",
      noPrivate: "Bez miejsca, notatki, adresu kanału i danych połączenia.",
      privateRemoved: "Prywatne lub nieznane pola usunięto z przepisu.",
      offlineDraft: "Rozłączono · szkic zostaje na tej stronie",
      pending: "Zmiana zlecona",
      duration: "Ostatnie odświeżenie",
      refreshSeconds: "s",
      deviceStatus: "Stan ekranu",
    },
  };
  const t = (k) => text[S.lang][k] || text.en[k] || k;
  const say = (en, pl) => (S.lang === "pl" ? pl : en);
  Object.assign(S, {
    editorOpen: false,
    settingsPage: "menu",
    setupDismissed: false,
    scan: { state: "idle", networks: [], error: "" },
    scanEpoch: 0,
    scanTimer: null,
    scanStarted: false,
    networkChoice: null,
    wifiStep: "list",
    wifiRequested: false,
    wifiStarted: 0,
    wifiTimedOut: false,
    search: {
      query: "",
      state: "idle",
      results: [],
      active: -1,
      message: "",
      epoch: 0,
      timer: null,
      controller: null,
    },
    ipPlace: null,
  });
  const iconPaths = {
    home: '<path d="m3 10 9-7 9 7v10a1 1 0 0 1-1 1h-5v-7H9v7H4a1 1 0 0 1-1-1z"/>',
    screens:
      '<rect x="3" y="4" width="18" height="15" rx="2"/><path d="M8 22h8M8 4v15"/>',
    clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
    settings:
      '<path d="M4 7h16M4 17h16"/><circle cx="8" cy="7" r="3"/><circle cx="16" cy="17" r="3"/>',
    share:
      '<path d="M12 16V3m-4 4 4-4 4 4M5 12v8a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-8"/>',
    wifi: '<path d="M3 8a15 15 0 0 1 18 0M6 12a10 10 0 0 1 12 0m-9 4a5 5 0 0 1 6 0"/><circle cx="12" cy="20" r=".8" fill="currentColor"/>',
    arrow: '<path d="m9 5 7 7-7 7"/>',
    back: '<path d="m14 5-7 7 7 7M7 12h14"/>',
    check: '<path d="m5 12 4 4L19 6"/>',
    refresh:
      '<path d="M20 7V3m0 4h-4M4 17v4m0-4h4M5 8a8 8 0 0 1 13-3l2 2M4 17l2 2a8 8 0 0 0 13-3"/>',
    eye: '<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/>',
    eyeOff:
      '<path d="m3 3 18 18M10 5c6-1 10 5 12 7a19 19 0 0 1-4 4M6 6a20 20 0 0 0-4 6s4 7 10 7c2 0 3-1 4-1M10 10a3 3 0 0 0 4 4"/>',
    sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5"/>',
    moon: '<path d="M20 14A9 9 0 0 1 10 4a9 9 0 1 0 10 10Z"/>',
    lock: '<rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3M12 14v3"/>',
    alert: '<path d="m12 3 10 18H2L12 3Z"/><path d="M12 9v5m0 3v1"/>',
    pause: '<path d="M8 5v14M16 5v14"/>',
    play: '<path d="m8 4 12 8-12 8V4Z"/>',
    edit: '<path d="m15 4 5 5M4 20l5-1L21 7l-5-5L4 14v6Z"/>',
    weather:
      '<path d="M8 6V3m-5 8H1m14-7 2-2M3 4l2 2"/><path d="M7 15a5 5 0 1 1 8-6M7 20h12a4 4 0 0 0 0-8 6 6 0 0 0-11-1 5 5 0 0 0-1 9Z"/>',
    feed: '<path d="M5 4h14v17H5V4ZM8 8h8m-8 4h8m-8 4h5"/>',
    note: '<path d="M4 3h16v13l-5 5H4V3Zm11 18v-5h5M8 8h8m-8 4h5"/>',
    sky: '<path d="M3 17a9 9 0 0 1 18 0"/><path d="M1 17h2m18 0h2M12 4v2M5.6 7.6 7 9m10.4-1.4L16 9"/><circle cx="12" cy="17" r="3"/>',
    air: '<path d="M3 8h11a3 3 0 1 0-3-3M3 12h15a3 3 0 1 1-3 3M3 16h8"/>',
    palette:
      '<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M12 3v18M3 12h18"/>',
    globe:
      '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c5 5 5 13 0 18-5-5-5-13 0-18Z"/>',
    info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v6m0-10v1"/>',
    close: '<path d="m6 6 12 12M6 18 18 6"/>',
  };
  function icon(name) {
    return `<svg class="ui-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${iconPaths[name] || iconPaths.info}</svg>`;
  }
  function signal(rssi) {
    const n = C.signalLevel(rssi);
    return `<svg class="ui-icon signal-icon" viewBox="0 0 24 24" aria-hidden="true">${[0, 1, 2].map((i) => `<rect x="${3 + i * 7}" y="${17 - i * 5}" width="4" height="${5 + i * 5}" rx="1" fill="currentColor" opacity="${i < n ? 1 : 0.2}"/>`).join("")}</svg>`;
  }
  function go(tab, subpage = null) {
    S.tab = tab;
    S.editorOpen = false;
    resetSearch();
    if (tab === "settings") S.settingsPage = subpage || "menu";
    if (subpage !== "wifi") stopScanPolling();
    render();
    window.scrollTo({ top: 0, behavior: "instant" });
    if (subpage === "wifi") startScan(false);
  }
  function backButton(action = "editor-back") {
    return `<button class="back-button" data-action="${action}">${icon("back")}<span>${say("Back", "Wróć")}</span></button>`;
  }
  function shouldOnboard() {
    return !!(
      S.config &&
      S.status?.setup &&
      !S.status?.online &&
      !S.setupDismissed
    );
  }
  function stopScanPolling() {
    clearTimeout(S.scanTimer);
    S.scanEpoch++;
  }
  function connectionHelp() {
    return `<details class="source-details"><summary>${say("Connection help", "Pomoc z połączeniem")}</summary><div class="source-state"><p>${say("On the device Wi-Fi “emini.ink”, use", "W sieci urządzenia „emini.ink” użyj")} <a href="http://192.168.4.1/" referrerpolicy="no-referrer">http://192.168.4.1</a>.</p><p>${say("Home’s home-network address is for your usual home Wi-Fi. Switch your phone to that network before opening it.", "Adres Home w domowej sieci jest przeznaczony dla domowego Wi-Fi. Przełącz telefon na tę sieć, zanim otworzysz ten adres.")}</p><a class="button-link" href="/">${say("Reload this local panel", "Wczytaj ten panel ponownie")}</a></div></details>`;
  }
  function lanAccessAction() {
    const access = C.lanHandoffState(
      location.hostname,
      S.status?.address,
      S.phoneOnHomeWiFi,
    );
    if (!access.url) return "";
    if (access.same)
      return `<button class="primary wide" data-action="wifi-finish">${say("Continue to Home", "Przejdź do Home")}${icon("arrow")}</button>`;
    if (!access.allowed)
      return `<p class="connection-help">${say("If your phone is still on “emini.ink”, switch it to your home Wi-Fi in its Wi-Fi settings, then return here.", "Jeśli telefon jest jeszcze w sieci „emini.ink”, przełącz go w ustawieniach Wi-Fi na sieć domową i wróć tutaj.")}</p><button class="primary wide" data-action="phone-network-changed">${say("My phone is on home Wi-Fi", "Telefon jest już w domowym Wi-Fi")}</button>`;
    return `<a class="button-link primary" href="${access.url}" target="_blank" rel="noopener noreferrer" referrerpolicy="no-referrer">${say("Open Home", "Otwórz Home")} · ${esc(S.status.address)}${icon("arrow")}</a>`;
  }
  iconPaths.more =
    '<circle cx="5" cy="12" r="1.2" fill="currentColor"/><circle cx="12" cy="12" r="1.2" fill="currentColor"/><circle cx="19" cy="12" r="1.2" fill="currentColor"/>';
  iconPaths.battery =
    '<rect x="2" y="7" width="18" height="10" rx="2"/><path d="M22 10v4"/>';
  function batteryWords() {
    const b = C.batteryView(S.status?.battery),
      state = {
        charging: say("Charging", "Ładowanie"),
        full: say("Fully charged", "Naładowana"),
        not_charging: say("Not charging", "Nie ładuje"),
        unknown: say("State unknown", "Stan nieznany"),
      }[b.kind];
    return {
      volts:
        b.volts === null
          ? "—"
          : new Intl.NumberFormat(S.lang, {
              minimumFractionDigits: 2,
              maximumFractionDigits: 2,
            }).format(b.volts) + " V",
      percent: b.percent === null ? "—" : "~" + b.percent + "%",
      state,
    };
  }
  function updateBattery() {
    const b = batteryWords(),
      summary = $("#battery-summary");
    if (summary) {
      summary.textContent = [
        say("Battery", "Bateria"),
        b.percent !== "—" ? b.percent : b.volts,
        b.state,
      ].join(" · ");
      const help = say(
        "Approximate charge from voltage. Runtime has not been measured.",
        "Przybliżone naładowanie z napięcia. Czas pracy nie został zmierzony.",
      );
      summary.title = help;
      summary.setAttribute("aria-label", summary.textContent + ". " + help);
    }
    const volts = $("#battery-voltage"),
      percent = $("#battery-percent"),
      state = $("#battery-state");
    if (volts) volts.textContent = b.volts;
    if (percent) percent.textContent = b.percent;
    if (state) state.textContent = b.state;
  }
  function batterySettings() {
    const b = batteryWords();
    return `<section class="settings-detail">${backButton("settings-back")}<div class="page-title"><span class="title-icon">${icon("battery")}</span><div><h1>${say("Battery", "Bateria")}</h1><p>${say("A measured voltage, an approximate charge level.", "Zmierzone napięcie i przybliżony poziom naładowania.")}</p></div></div><div class="device-summary"><dl class="device-facts"><dt>${say("Voltage", "Napięcie")}</dt><dd id="battery-voltage">${esc(b.volts)}</dd><dt>${say("Estimated charge", "Szacowane naładowanie")}</dt><dd id="battery-percent">${esc(b.percent)}</dd><dt>${say("Charging state", "Stan ładowania")}</dt><dd id="battery-state">${esc(b.state)}</dd></dl></div><p class="hint">${say("Charge is estimated from voltage when Home is not charging. Runtime has not been measured yet.", "Poziom jest szacowany z napięcia, gdy Home nie ładuje. Czas pracy nie został jeszcze zmierzony.")}</p></section>`;
  }
  async function previewAppearance(button) {
    if (S.previewBusy) return;
    const screen = S.selected,
      signature = JSON.stringify(S.draft),
      dialogEpoch = S.dialogEpoch || 0;
    if (C.validate(S.draft).length) {
      notice(t("errorFields"), true);
      return;
    }
    S.previewBusy = true;
    if (button) {
      button.disabled = true;
      button.innerHTML =
        icon("refresh") + say("Preparing preview…", "Przygotowuję podgląd…");
    }
    try {
      if (S.dirty) {
        const cached = S.draftFrames[screen];
        if (!cached || cached.signature !== signature) {
          let buffer;
          if (
            S.previewWork?.screen === screen &&
            S.previewWork.signature === signature
          ) {
            buffer = await S.previewWork.promise;
          } else {
            clearTimeout(S.previewTimer);
            S.previewEpoch++;
            S.previewController?.abort();
            const result = await request("/api/preview?screen=" + screen, {
              method: "POST",
              body: C.previewConfig(S.draft),
              binary: true,
            });
            buffer = result.buffer;
          }
          C.decodeFrame(buffer);
          if (screen !== S.selected || signature !== JSON.stringify(S.draft))
            return;
          S.draftFrames[screen] = { buffer, signature };
        }
      } else if (!S.frames[screen]) {
        const result = await request("/api/preview?screen=" + screen, {
          binary: true,
        });
        C.decodeFrame(result.buffer);
        S.frames[screen] = result.buffer;
      }
      if (
        S.tab !== "settings" ||
        S.settingsPage !== "appearance" ||
        dialogEpoch !== (S.dialogEpoch || 0)
      )
        return;
      paintAll();
      nativePreview("draft");
    } catch (e) {
      if (e.name !== "AbortError") error(e);
    } finally {
      S.previewBusy = false;
      if (button) {
        button.disabled = false;
        button.innerHTML =
          icon("screens") + say("Preview · 400 × 300", "Podgląd · 400 × 300");
      }
    }
  }
  function discardMenu() {
    dialog(
      `<h2>${say("Your changes", "Twoje zmiany")}</h2><p>${say("Keep editing, or return to the settings you last loaded from Home.", "Kontynuuj edycję albo wróć do ustawień ostatnio wczytanych z Home.")}</p><div class="fields"><button data-modal="cancel">${say("Keep editing", "Edytuj dalej")}</button><button id="discard-draft">${say("Discard changes", "Odrzuć zmiany")}</button></div>`,
    );
    $("#discard-draft").onclick = () => {
      $("#review").close();
      clearTimeout(S.previewTimer);
      S.previewEpoch++;
      S.previewController?.abort();
      S.draft = C.clone(S.config);
      resetSearch();
      S.draftFrames = {};
      S.dirty = false;
      render();
      notice(
        say("Unsaved changes discarded.", "Niezapisane zmiany odrzucone."),
      );
    };
  }
  function setupStepLabels() {
    const done =
        C.wifiOutcome(
          S.status,
          S.networkChoice?.ssid,
          S.wifiAwaitingStatus,
          S.wifiTimedOut,
        ) === "connected",
      ready =
        done &&
        C.lanHandoffState(location.hostname, S.status?.address, false).same;
    return `<span class="done">${icon("check")}${say("Phone", "Telefon")}</span><span class="${done ? "done" : "current"}">${done ? icon("check") : "2 ·"} Wi-Fi</span><span class="${ready ? "done" : done ? "current" : ""}">${ready ? icon("check") : "3 ·"} ${say("Open Home", "Otwórz Home")}</span>`;
  }
  async function showChosen(screen) {
    if (S.dirty) {
      notice(t("savingBlocked"), true);
      return;
    }
    if (
      !C.screens.includes(screen) ||
      !S.config.enabled[C.screens.indexOf(screen)]
    )
      return;
    await mutate("/api/show", { screen });
    notice(t("showAccepted"));
    if (S.status) S.status.pending = true;
    updateStatus();
    setTimeout(poll, 1000);
  }
  function sourceLabel(screen) {
    if (screen === "note")
      return S.config?.note
        ? say("Your words", "Twoje słowa")
        : say("Add your words", "Dodaj swoje słowa");
    if (screen === "sky")
      return S.config?.location_ready === false
        ? say("Needs your location", "Potrzebuje lokalizacji")
        : say("Computed on the device", "Liczone na urządzeniu");
    const state = S.status?.sources?.[screen]?.state;
    const key = {
      ready: "fresh",
      fresh: "fresh",
      stale: "stale",
      error: "failed",
      failed: "failed",
      empty: "empty",
      disabled: "disabled",
      "unknown-time": "unknown",
    }[state];
    return key ? t(key) : say("Loading status…", "Wczytywanie stanu…");
  }
  function updateHomeCards() {
    document.querySelectorAll("[data-poster-screen]").forEach((card) => {
      const screen = card.dataset.posterScreen,
        index = C.screens.indexOf(screen);
      if (index < 0) return;
      const active = S.status?.displayed_screen === screen;
      const copy =
        S.draft?.enabled[index] === false ? t("off") : sourceLabel(screen);
      const label = card.querySelector("[data-source-label]");
      if (label && label.textContent !== copy) label.textContent = copy;
      card.classList.toggle("on-device", active);
      const badge = card.querySelector("[data-on-device]");
      if (badge) badge.hidden = !active;
    });
  }
  function safeStoryURL(value, feedURL) {
    if (
      typeof value !== "string" ||
      value.length > 2048 ||
      /[\u0000-\u0020\u007f\\]/.test(value)
    )
      return null;
    try {
      const url = new URL(value);
      if (
        url.protocol !== "https:" ||
        !url.hostname ||
        url.username ||
        url.password
      )
        return null;
      if (typeof feedURL === "string" && feedURL) {
        try {
          const feed = new URL(feedURL),
            article = new URL(url.href);
          feed.hash = "";
          article.hash = "";
          if (feed.href === article.href) return null;
        } catch {
          /* An invalid configured feed is not an article fallback. */
        }
      }
      return url.href;
    } catch {
      return null;
    }
  }
  function updateSelectedStory() {
    const holder = document.querySelector("#selected-story");
    if (!holder) return;
    const item = S.status?.sources?.feed,
      url = safeStoryURL(item?.url, S.config?.feed_url);
    holder.hidden = !url;
    if (!url) return;
    holder.querySelector("[data-story-title]").textContent =
      typeof item.title === "string" ? item.title : "";
    const source = typeof item.source === "string" ? item.source : "";
    const published =
      Number.isFinite(item.published_at) && item.published_at > 0
        ? formatTime(item.published_at)
        : "";
    holder.querySelector("[data-story-source]").textContent = [
      source,
      published,
    ]
      .filter(Boolean)
      .join(" · ");
    const link = holder.querySelector("[data-story-link]");
    link.href = url;
    link.textContent = say("Read full story ↗", "Przeczytaj wiadomość ↗");
  }
  function modeSummary() {
    const c = S.config;
    if (!c) return "";
    if (S.status?.pause_remaining > 0)
      return (
        say("Paused for ", "Pauza na ") +
        Math.ceil(S.status.pause_remaining / 60) +
        say(" min", " min")
      );
    return c.mode === "rotate"
      ? say("Every ", "Co ") + c.interval_min + " min"
      : c.mode === "day"
        ? say("Three moments in your day", "Trzy chwile w Twoim dniu")
        : say("One screen: ", "Jeden ekran: ") + t(c.fixed_screen);
  }
  function navigation() {
    return `<nav class="tabs app-nav" role="tablist" aria-label="${say("Main navigation", "Nawigacja główna")}">${[
      ["screens", "home", say("Home", "Home")],
      ["rhythm", "clock", t("rhythm")],
      ["settings", "settings", t("settings")],
      ["share", "share", say("Share", "Udostępnij")],
    ]
      .map(
        ([key, pic, label]) =>
          `<button id="tab-${key}" role="tab" aria-controls="tab-panel" aria-selected="${S.tab === key}" tabindex="${S.tab === key ? "0" : "-1"}" data-action="tab" data-tab="${key}">${icon(pic)}<span>${label}</span></button>`,
      )
      .join("")}</nav>`;
  }
  function overview() {
    return `<div class="home-overview"><section class="now-section"><div class="now-heading"><div><p class="eyebrow">${say("ON YOUR HOME", "NA TWOIM HOME")}</p><h1>${esc(S.config.name)}</h1></div><span class="state-chip" id="display-state">${statusName()}</span></div><figure class="now-art"><div class="device-frame" id="confirmed-holder">${S.frame ? `<canvas id="confirmed-frame" width="400" height="300" role="img" aria-label="${esc(t("confirmed"))}"></canvas>` : `<div class="frame-placeholder">${icon("screens")}<strong>${t("unconfirmed")}</strong><small id="frame-message">${t("frameUnknown")}</small></div>`}</div><figcaption><span id="frame-label">${t("confirmed")}</span><button class="icon-label-button" data-action="native-preview" data-kind="confirmed" ${!S.frame ? "disabled" : ""}>${icon("screens")}<span>1:1</span></button></figcaption></figure><p class="hint" id="battery-summary"></p><p class="refresh-note" id="refresh-note">${t("frameHint")}</p><button class="rhythm-summary" data-action="open-rhythm">${icon(S.status?.pause_remaining > 0 ? "pause" : "clock")}<span><strong>${t("rhythm")}</strong><small id="mode-summary">${esc(modeSummary())}</small></span>${icon("arrow")}</button>${!S.status?.online ? `<button class="network-prompt" data-action="wifi-settings">${icon("wifi")}<span>${say("Connect Wi-Fi for live information", "Połącz Wi-Fi, aby mieć aktualne informacje")}</span>${icon("arrow")}</button>` : ""}</section><section class="screen-library"><div class="section-label"><div><p class="eyebrow">${say("MAKE IT YOURS", "PO TWOJEMU")}</p><h2>${t("yourScreens")}</h2></div><span class="small muted">${S.config.enabled.filter(Boolean).length}/${C.screens.length} ${say("active", "aktywne")}</span></div><div class="poster-list">${S.draft.order.map((key) => `<article class="poster-card ${S.status?.displayed_screen === key ? "on-device" : ""}" data-poster-screen="${key}"><button class="poster-open" data-action="select" data-screen="${key}"><span class="poster-thumb"><canvas data-screen="${key}" width="400" height="300" aria-hidden="true"></canvas></span><span class="poster-copy"><span class="poster-name">${icon(key)}${t(key)}</span><small data-source-label>${!S.draft.enabled[C.screens.indexOf(key)] ? t("off") : esc(sourceLabel(key))}</small><span class="on-device-label" data-on-device ${S.status?.displayed_screen === key ? "" : "hidden"}>${icon("check")}${t("active")}</span></span>${icon("arrow")}</button><div class="poster-actions"><button data-action="select" data-screen="${key}">${icon("edit")}${t("edit")}</button><button data-action="show-card" data-screen="${key}" ${S.dirty || !S.config.enabled[C.screens.indexOf(key)] ? "disabled" : ""}>${icon("play")}${t("show")}</button></div></article>`).join("")}</div></section></div>`;
  }
  function editor() {
    const s = S.selected,
      index = C.screens.indexOf(s),
      position = S.draft.order.indexOf(s),
      last =
        S.draft.enabled.filter(Boolean).length === 1 && S.draft.enabled[index];
    return `<section class="editor-page">${backButton()}<div class="page-title"><span class="title-icon">${icon(s)}</span><div><h1>${t(s)}</h1><p>${t(s + "Desc")}</p></div></div><div class="editor-layout"><section class="editor-fields"><div class="form-section"><h2>${say("What it says", "Co pokazuje")}</h2>${s === "feed" ? `<section id="selected-story" class="source-state" hidden><strong data-story-title></strong><p class="hint" data-story-source></p><a class="button-link" data-story-link target="_blank" rel="noopener noreferrer" referrerpolicy="no-referrer">${say("Read full story ↗", "Przeczytaj wiadomość ↗")}</a></section>` : ""}<div class="fields">${
      s === "weather"
        ? weatherEditor()
        : s === "feed"
          ? field(
              "feed_url",
              "feedURL",
              "url",
              'maxlength="512" inputmode="url" placeholder="https://…" autocomplete="off"',
              "feedHelp",
            )
          : s === "sky"
            ? `<p class="hint">${t("skySource")}</p>`
            : s === "air"
              ? `<p class="hint">${t("airSource")}</p>${select(
                  "air_main",
                  "airMain",
                  [
                    ["eu", t("airEU")],
                    ["us", t("airUS")],
                    ["pm25", t("airPM")],
                  ],
                )}`
              : `<label class="field"><span>${t("noteText")}</span><textarea data-path="note" maxlength="240" rows="5" placeholder="${say("What matters today?", "Co jest dziś ważne?")}">${esc(S.draft.note)}</textarea><small>${t("noteHelp")}<span id="note-count">${noteSpace(S.draft.note)}</span><progress id="note-meter" max="100" value="${Math.min(100, C.noteUsage(S.draft.note).percent)}" aria-label="${esc(noteSpace(S.draft.note))}"></progress></small></label>`
    }</div></div><div class="form-section"><h2>${say("How it looks", "Jak wygląda")}</h2><div class="fields">${select(
      "styles." + s,
      "layout",
      [
        ["print", t("print")],
        ["rhythm", t("rhythmStyle")],
        ["atlas", t("atlas")],
        ["cycle", t("cycleStyle")],
      ],
    )}${styleHints(s)}<button class="setting-link" data-action="appearance-settings">${icon("palette")}<span>${say("Texture, colour and larger text", "Faktura, kolor i większy tekst")}</span>${icon("arrow")}</button></div></div><div class="form-section"><h2>${say("In your collection", "W Twojej kolekcji")}</h2><label class="check"><span>${t("enabled")}</span><input type="checkbox" data-path="enabled.${index}" ${S.draft.enabled[index] ? "checked" : ""} ${last ? "disabled" : ""}></label>${last ? `<p class="hint">${say("Keep at least one screen enabled.", "Co najmniej jeden ekran musi pozostać aktywny.")}</p>` : ""}<div class="order-control"><button data-action="move" data-direction="-1" ${position === 0 ? "disabled" : ""}>↑ ${t("earlier")}</button><span>${position + 1} / ${C.screens.length}</span><button data-action="move" data-direction="1" ${position === C.screens.length - 1 ? "disabled" : ""}>↓ ${t("later")}</button></div></div></section><aside class="editor-aside"><section class="edit-preview"><div class="preview-title"><h2 id="preview-heading">${t("savedPreview")}</h2><button class="icon-label-button" data-action="native-preview" data-kind="draft">${icon("screens")}<span>1:1</span></button></div><canvas data-screen="${s}" width="400" height="300" role="img" aria-label="${esc(t("savedPreview"))}" ${!S.frames[s] ? "hidden" : ""}></canvas><p class="hint" id="preview-missing" ${S.frames[s] ? "hidden" : ""}>${t("previewMissing")}</p><p class="hint" id="draft-preview" ${!S.dirty ? "hidden" : ""}>${t("draftPreview")}</p><p class="hint">${t("photoHint")}</p></section><div id="source-holder">${sourceBlock(s)}</div></aside></div></section>`;
  }
  function settingsMenu() {
    const rows = [
      [
        "wifi",
        "wifi",
        say("Wi-Fi connection", "Połączenie Wi-Fi"),
        S.status?.wifi?.ssid ||
          (S.status?.online
            ? say("Connected", "Połączono")
            : say("Choose your network", "Wybierz swoją sieć")),
      ],
      ["battery", "battery", say("Battery", "Bateria"), batteryWords().state],
      [
        "appearance",
        "palette",
        say("Appearance", "Wygląd"),
        say("Pixel texture, colour, text", "Faktura pikseli, kolory, tekst"),
      ],
      [
        "preferences",
        "globe",
        say("Preferences", "Preferencje"),
        say("Language, time zone and units", "Język, strefa czasu i jednostki"),
      ],
      [
        "device",
        "info",
        say("Your device", "Twoje urządzenie"),
        say("Connection details and help", "Szczegóły połączenia i pomoc"),
      ],
    ];
    return `<section class="settings-menu"><div class="page-title"><div><p class="eyebrow">${esc(S.config.name)}</p><h1>${t("settings")}</h1><p>${say("A few things that make it yours.", "Kilka rzeczy, które robią różnicę.")}</p></div></div><div class="settings-menu-list">${rows.map(([page, pic, title, subtitle]) => `<button class="settings-menu-item" data-action="settings-page" data-page="${page}"><span class="menu-icon">${icon(pic)}</span><span><strong>${title}</strong><small>${esc(subtitle)}</small></span>${icon("arrow")}</button>`).join("")}</div></section>`;
  }
  function appearanceSettings() {
    return `<section class="settings-detail appearance-detail">${backButton("settings-back")}<div class="page-title"><span class="title-icon">${icon("palette")}</span><div><h1>${say("Appearance", "Wygląd")}</h1><p>${say("Four pigments. Your own character.", "Cztery pigmenty. Własny charakter.")}</p></div></div><div class="appearance-layout"><div><div class="form-section"><h2>${t("texture")}</h2><div class="texture-tiles">${[
      [
        1,
        say("Fine", "Drobna"),
        say("1 px · colour 2 px", "1 px · kolor 2 px"),
      ],
      [2, say("Medium", "Średnia"), "2 px"],
      [4, say("Large", "Duża"), "4 px"],
    ]
      .map(
        ([n, label, size]) =>
          `<button data-action="texture-choice" data-value="${n}" aria-pressed="${S.draft.texture === n}"><span class="texture-swatch texture-${n}" aria-hidden="true"></span><strong>${label}</strong><small>${size}</small></button>`,
      )
      .join(
        "",
      )}</div><p class="hint">${say("Colour never goes below 2 px, so Fine and Medium differ only in black patterns.", "Kolor nie schodzi poniżej 2 px, dlatego Drobna i Średnia różnią się tylko czarnymi wzorami.")}</p></div><div class="form-section"><h2>${say("Colour use", "Użycie kolorów")}</h2><div class="colour-tiles">${[
      [0, t("soft")],
      [1, t("balanced")],
      [2, t("bold")],
    ]
      .map(
        ([n, label]) =>
          `<button data-action="intensity-choice" data-value="${n}" aria-pressed="${S.draft.intensity === n}"><span class="colour-swatch colour-${n}" aria-hidden="true"></span><strong>${label}</strong></button>`,
      )
      .join(
        "",
      )}</div></div><div class="form-section"><h2>${t("brush")}</h2>${select(
      "brush",
      "brushChoice",
      [
        ["grain", t("brushGrain")],
        ["halftone", t("brushHalftone")],
        ["grid", t("brushGrid")],
      ],
    )}<p class="hint">${t("brushHelp")}</p>${check("large_text", "largeText")}</div></div><section class="appearance-preview edit-preview"><div class="preview-title"><h2 id="preview-heading">${t("savedPreview")}</h2></div><label class="field"><span>${say("Preview screen", "Podgląd ekranu")}</span><select id="appearance-screen">${options(screenOptions(S.selected), S.selected)}</select></label><canvas data-screen="${S.selected}" width="400" height="300" role="img" aria-label="${esc(t("savedPreview"))}" ${!S.frames[S.selected] ? "hidden" : ""}></canvas><p class="hint" id="preview-missing" ${S.frames[S.selected] ? "hidden" : ""}>${t("previewMissing")}</p><p class="hint" id="draft-preview" ${!S.dirty ? "hidden" : ""}>${t("draftPreview")}</p><button class="primary wide" data-action="appearance-preview">${icon("screens")}${say("Preview · 400 × 300", "Podgląd · 400 × 300")}</button><p class="hint">${say("This preview does not change your Home display. Save and Show remain separate.", "Ten podgląd nie zmienia obrazu na Home. Zapisz i Pokaż pozostają osobnymi krokami.")}</p></section></div><div class="form-section"><h2>${t("profile")}</h2><div class="profile-grid">${[
      ["desk", "note"],
      ["distance", "screens"],
      ["showcase", "palette"],
    ]
      .map(
        ([p, i]) =>
          `<button data-action="profile" data-profile="${p}">${icon(i)}<strong>${t(p)}</strong></button>`,
      )
      .join("")}</div><p class="hint">${t("profileHelp")}</p></div></section>`;
  }
  function preferencesSettings() {
    return `<section class="settings-detail">${backButton("settings-back")}<div class="page-title"><span class="title-icon">${icon("globe")}</span><div><h1>${say("Preferences", "Preferencje")}</h1><p>${say("A familiar language. The right time.", "Znajomy język. Właściwa godzina.")}</p></div></div><div class="form-section fields">${field("name", "name", "text", 'maxlength="48" autocomplete="off"')}${select(
      "locale",
      "language",
      [
        ["en", "English"],
        ["pl", "Polski"],
      ],
    )}${select("ok_action", "okAction", [
      ["language", t("okLanguage")],
      ["refresh", t("okRefresh")],
      ["hold", t("okHold")],
      ["setup", t("okSetup")],
    ])}<p class="hint">${t("okHelp")}</p>${select("units", "units", [
      ["C", "°C"],
      ["F", "°F"],
    ])}${select(
      "timezone",
      "timezone",
      C.zones.map((z) => [
        z,
        zoneLabels[z] || z.replaceAll("_", " ").replace("/", " / "),
      ]),
    )}${check("clock24", "clock")}</div></section>`;
  }
  function deviceSettings() {
    return `<section class="settings-detail">${backButton("settings-back")}<div class="page-title"><span class="title-icon">${icon("info")}</span><div><h1>${t("device")}</h1><p>${esc(S.config.name)}</p></div></div><div class="device-summary"><dl class="device-facts"><dt>${t("version")}</dt><dd>${esc(S.status?.version || "—")}</dd><dt>${t("address")}</dt><dd>${esc(S.status?.address || location.host)}</dd><dt>${t("deviceStatus")}</dt><dd id="settings-state">${statusName()}</dd><dt>${t("generation")}</dt><dd id="settings-generation">${esc(S.frameGeneration ?? "—")}</dd><dt>${t("duration")}</dt><dd>${S.status?.refresh_ms ? (S.status.refresh_ms / 1000).toFixed(1) + " s" : "—"}</dd></dl></div><div class="help-section"><h2>${say("Connect another phone", "Połącz kolejny telefon")}</h2><p>${t("pairNote")}</p><p>${say("Use the same local address on each visit. Your phone remembers access for that address.", "Korzystaj za każdym razem z tego samego lokalnego adresu. Telefon zapamiętuje dostęp do tego adresu.")}</p></div><button class="disconnect-button" data-action="unpair">${t("forget")}</button></section>`;
  }
  function wifiError(code) {
    return (
      {
        configuration_failed: say(
          "Home could not save the network settings. Try again.",
          "Home nie zapisał ustawień sieci. Spróbuj ponownie.",
        ),
        authentication_failed: say(
          "Home could not sign in to this network. Check the password and try again.",
          "Home nie zalogował się do tej sieci. Sprawdź hasło i spróbuj ponownie.",
        ),
        network_not_found: say(
          "Home cannot find this network now. Move it closer or choose another network.",
          "Home nie widzi teraz tej sieci. Przenieś go bliżej lub wybierz inną sieć.",
        ),
        connection_lost: say(
          "The Wi-Fi connection was lost. Check your router or choose another network.",
          "Połączenie Wi-Fi zostało przerwane. Sprawdź router lub wybierz inną sieć.",
        ),
      }[code] ||
      say(
        "The connection has not been confirmed. You can check the password or choose another network.",
        "Połączenie nie zostało potwierdzone. Możesz sprawdzić hasło lub wybrać inną sieć.",
      )
    );
  }
  function wifiList() {
    const scan = S.scan,
      waiting = scan.state === "scanning";
    return `<div class="wifi-list-heading"><h2>${say("Nearby networks", "Sieci w pobliżu")}</h2><button class="icon-label-button" data-action="wifi-scan" ${waiting ? "disabled" : ""}>${icon("refresh")}<span>${say("Scan", "Szukaj")}</span></button></div>${waiting ? `<div class="scan-status" role="status">${icon("wifi")}<span>${say("Looking for nearby networks…", "Szukam sieci w pobliżu…")}</span></div>` : ""}${scan.state === "error" ? `<div class="inline-feedback error" role="alert">${icon("alert")}<div><strong>${say("Scan unavailable", "Skanowanie niedostępne")}</strong><p>${say("Try again. You can also enter a hidden network below.", "Spróbuj ponownie. Możesz też wpisać ukrytą sieć poniżej.")}</p></div></div>` : ""}<div class="network-list">${scan.networks.map((n, i) => `<button class="network-row" data-action="wifi-choose" data-network="${i}" ${!n.supported || waiting ? "disabled" : ""}><span class="network-signal">${signal(n.rssi)}</span><span class="network-info"><strong>${esc(n.ssid)}</strong><small>${n.connected ? say("Connected", "Połączono") : !n.supported ? (n.secure ? say("WPA2/WPA3 Personal required", "Wymagane WPA2/WPA3 Personal") : say("Open networks are not supported", "Sieci otwarte nie są obsługiwane")) : C.signalLevel(n.rssi) === 3 ? say("Strong signal", "Silny sygnał") : C.signalLevel(n.rssi) === 2 ? say("Good signal", "Dobry sygnał") : say("Weak signal", "Słaby sygnał")}</small></span><span class="network-security">${icon(n.connected ? "check" : n.secure ? "lock" : "info")}</span>${n.supported && !n.connected ? icon("arrow") : ""}</button>`).join("")}</div>${scan.state === "ready" && !scan.networks.length ? `<div class="empty-network">${icon("wifi")}<h3>${say("No networks found", "Nie znaleziono sieci")}</h3><p>${say("Move Home closer to your router, then scan again.", "Przenieś Home bliżej routera i spróbuj ponownie.")}</p></div>` : ""}<button class="hidden-network" data-action="wifi-manual">${say("Hidden network? Enter its name", "Ukryta sieć? Wpisz jej nazwę")}${icon("arrow")}</button>`;
  }
  function wifiPassword() {
    const manual = S.wifiStep === "manual",
      n = S.networkChoice;
    return `<div class="network-selected"><span class="network-selected-icon">${icon("wifi")}</span><div><strong>${manual ? say("Hidden network", "Ukryta sieć") : esc(n?.ssid || "")}</strong><small>2.4 GHz · WPA2 / WPA3 Personal</small></div></div><form class="wifi-form" id="wifi-form" autocomplete="off">${manual ? `<label class="field"><span>${t("ssid")}</span><input name="ssid" maxlength="32" required autocomplete="off" autocapitalize="none" spellcheck="false" placeholder="${say("Exact network name", "Dokładna nazwa sieci")}"></label>` : `<input type="hidden" name="ssid" value="${esc(n?.ssid || "")}">`}<label class="field"><span>${t("password")}</span><span class="password-field"><input name="password" id="wifi-password" type="password" minlength="8" maxlength="63" required autocomplete="new-password" placeholder="${say("Password for this network", "Hasło do tej sieci")}"><button type="button" data-action="password-toggle" aria-label="${say("Show password", "Pokaż hasło")}" aria-pressed="false">${icon("eye")}</button></span></label><p class="hint">${say("This goes directly to your Home. It is never included in a recipe or sent to emini.ink.", "Hasło trafia bezpośrednio do Twojego Home. Nie jest częścią przepisu ani nie trafia do emini.ink.")}</p><button type="submit" class="primary wide">${icon("wifi")}${say("Connect Home", "Połącz Home")}</button><button type="button" class="text" data-action="wifi-list">${say("Choose another network", "Wybierz inną sieć")}</button></form>`;
  }
  function wifiProgress() {
    const outcome = C.wifiOutcome(
        S.status,
        S.networkChoice?.ssid,
        S.wifiAwaitingStatus,
        S.wifiTimedOut,
      ),
      confirmed = outcome === "connected",
      failed = outcome === "error" || outcome === "unconfirmed";
    if (confirmed) {
      const url = C.localDeviceURL(S.status?.address),
        same = url && new URL(url).hostname === location.hostname;
      return `<div class="wifi-result wifi-success"><span class="result-icon success">${icon("check")}</span><h1>${say("Home is connected.", "Home jest połączony.")}</h1><p>${esc(S.status.wifi.ssid || S.networkChoice?.ssid || "")}</p>${url ? `<div class="connection-address"><small>${say("Your Home address", "Adres Twojego Home")}</small><strong>${esc(S.status.address)}</strong></div>` : ""}${lanAccessAction()}<p class="connection-help">${same ? say("You can now choose what stays on the screen.", "Teraz możesz wybrać, co zostaje na ekranie.") : say("At the new address, the panel always asks for a pairing code. The code is valid for 5\u00a0minutes. If it has expired, hold OK\u00a0/\u00a0BOOT on Home for 2\u00a0seconds. A new code appears on the screen after about 30\u00a0seconds.", "Pod nowym adresem panel zawsze poprosi o kod parowania. Kod jest ważny 5\u00a0minut. Gdy wygaśnie, przytrzymaj OK\u00a0/\u00a0BOOT na Home przez 2\u00a0sekundy. Nowy kod pojawi się na ekranie po około 30\u00a0sekundach.")}</p></div>`;
    }
    return `<div class="wifi-result"><span class="result-icon ${failed ? "problem" : ""}">${icon(failed ? "alert" : "wifi")}</span><h2>${failed ? say("Let’s try that again.", "Spróbujmy jeszcze raz.") : say("Home is connecting…", "Home łączy się z siecią…")}</h2><p class="network-name">${esc(S.networkChoice?.ssid || "")}</p><p>${failed ? wifiError(outcome === "error" ? S.status?.wifi?.error : null) : say("This can take a moment. Your phone may briefly lose contact while Home joins the network.", "To może potrwać chwilę. Telefon może na moment stracić kontakt, gdy Home dołącza do sieci.")}</p><div class="connection-steps"><span>${icon("check")}${say("Settings received", "Ustawienia odebrane")}</span><span>${icon("wifi")}${say("Waiting for Wi-Fi confirmation", "Oczekiwanie na potwierdzenie Wi-Fi")}</span></div>${failed ? `<div class="fields"><button class="primary" data-action="wifi-retry">${say("Check password", "Sprawdź hasło")}</button><button data-action="wifi-list">${say("Choose another network", "Wybierz inną sieć")}</button></div>` : `<p class="hint">${say("If this page disconnects, check the address on Home and reconnect your phone to your home Wi-Fi.", "Jeśli ta strona się rozłączy, sprawdź adres na ekranie Home i połącz telefon z domowym Wi-Fi.")}</p><button class="text" data-action="wifi-list">${say("Back to networks", "Wróć do sieci")}</button>`}</div>`;
  }
  function wifiView(onboarding = false) {
    return `<section class="wifi-page" id="wifi-panel">${!onboarding ? backButton("settings-back") : `<div class="setup-steps">${setupStepLabels()}</div>`}<div class="page-title"><span class="title-icon">${icon("wifi")}</span><div><h1>${say("Your home Wi-Fi", "Domowe Wi-Fi")}</h1><p>${say("A direct connection. A live picture.", "Bezpośrednie połączenie. Aktualny obraz.")}</p></div></div><div id="wifi-content">${["password", "manual"].includes(S.wifiStep) ? wifiPassword() : S.wifiStep === "connecting" ? wifiProgress() : wifiList()}</div>${S.wifiStep === "list" ? `<div class="wifi-band-note">${icon("info")}<p>${say("Home uses 2.4 GHz Wi-Fi. Choose your usual network if your router shares one name across both bands.", "Home korzysta z Wi-Fi 2,4 GHz. Wybierz swoją zwykłą sieć, jeśli router używa jednej nazwy dla obu pasm.")}</p></div>` : ""}${onboarding && S.wifiStep === "list" ? `<button class="text setup-later" data-action="wifi-later">${say("Set up later", "Skonfiguruj później")}</button>` : ""}</section>`;
  }
  function updateWifiContent() {
    const content = $("#wifi-content");
    if (!content) return;
    const done =
      S.wifiStep === "connecting" &&
      C.wifiOutcome(
        S.status,
        S.networkChoice?.ssid,
        S.wifiAwaitingStatus,
        S.wifiTimedOut,
      ) === "connected";
    $("#wifi-panel")?.classList.toggle("wifi-connected", done);
    const steps = $(".setup-steps");
    if (steps) steps.innerHTML = setupStepLabels();
    if (S.wifiStep === "list") content.innerHTML = wifiList();
    else if (S.wifiStep === "connecting") content.innerHTML = wifiProgress();
  }
  async function startScan(force = false) {
    if (!S.token) return;
    if (
      !force &&
      S.scanStarted &&
      S.scan.state === "ready" &&
      Date.now() - (S.scanAt || 0) < 30000
    )
      return;
    const resume = !force && S.scanStarted && S.scan.state === "scanning";
    stopScanPolling();
    const epoch = S.scanEpoch;
    S.scanStarted = true;
    S.scan = { state: "scanning", networks: [], error: "" };
    updateWifiContent();
    const until = performance.now() + 22000;
    try {
      if (!resume) await mutate("/api/wifi/scan", {});
      async function readScan() {
        if (
          epoch !== S.scanEpoch ||
          !S.token ||
          !$("#wifi-panel") ||
          S.wifiStep !== "list"
        )
          return;
        try {
          S.scan = C.normalizeNetworks(await request("/api/wifi/scan"));
          if (epoch !== S.scanEpoch) return;
          updateWifiContent();
          if (S.scan.state === "ready") {
            S.scanAt = Date.now();
            return;
          }
          if (S.scan.state === "error") return;
          if (performance.now() > until) {
            S.scan = { state: "error", networks: [], error: "timeout" };
            updateWifiContent();
            return;
          }
          S.scanTimer = setTimeout(readScan, 1000);
        } catch (e) {
          if (e.status === 401) return;
          S.scan = { state: "error", networks: [], error: "unavailable" };
          updateWifiContent();
        }
      }
      await readScan();
    } catch (e) {
      if (e.status === 401) return;
      S.scan = { state: "error", networks: [], error: "unavailable" };
      updateWifiContent();
    }
  }
  function beginWifiConfirmation() {
    S.wifiStarted = performance.now();
    S.wifiTimedOut = false;
    clearTimeout(S.wifiTimer);
    async function read() {
      if (!S.token || S.wifiStep !== "connecting") return;
      await poll();
      const outcome = C.wifiOutcome(
        S.status,
        S.networkChoice?.ssid,
        S.wifiAwaitingStatus,
        S.wifiTimedOut,
      );
      if (outcome === "connected" || outcome === "error") {
        updateWifiContent();
        return;
      }
      if (performance.now() - S.wifiStarted > 45000) {
        S.wifiTimedOut = true;
        updateWifiContent();
        return;
      }
      S.wifiTimer = setTimeout(read, 1500);
    }
    S.wifiTimer = setTimeout(read, 500);
  }
  const esc = (v) =>
    String(v ?? "").replace(
      /[&<>"']/g,
      (c) =>
        ({
          "&": "&amp;",
          "<": "&lt;",
          ">": "&gt;",
          '"': "&quot;",
          "'": "&#39;",
        })[c],
    );
  const val = (path) => path.split(".").reduce((v, k) => v?.[k], S.draft);
  const options = (list, value) =>
    list
      .map(
        ([v, l]) =>
          `<option value="${esc(v)}" ${v === value ? "selected" : ""}>${esc(l)}</option>`,
      )
      .join("");
  const field = (path, label, type = "text", extra = "", help = "") =>
    `<label class="field"><span>${esc(t(label))}</span><input data-path="${path}" type="${type}" value="${esc(val(path))}" ${extra}>${help ? `<small>${esc(t(help))}</small>` : ""}</label>`;
  const select = (path, label, list) =>
    `<label class="field"><span>${esc(t(label))}</span><select data-path="${path}">${options(list, val(path))}</select></label>`;
  const check = (path, label) =>
    `<label class="check"><span>${esc(t(label))}</span><input type="checkbox" data-path="${path}" ${val(path) ? "checked" : ""}></label>`;
  /* "Day rhythm" and "One screen" choose among the screens in the collection;
     a screen already saved in the slot stays selectable until it is changed. */
  const screenOptions = (current) =>
    C.screens
      .filter((s, i) => S.draft?.enabled?.[i] === true || s === current)
      .map((s) => [s, t(s)]);
  let zoneLabels = {},
    zonesFetched = false;
  async function loadTimezones() {
    if (zonesFetched || !S.token) return;
    try {
      const response = await request("/api/timezones");
      if (!Array.isArray(response) || !response.length || response.length > 600)
        return;
      const rows = response.map((z) =>
        typeof z === "string" ? { name: z, label: z } : z,
      );
      if (
        rows.some(
          (z) =>
            !z ||
            typeof z.name !== "string" ||
            z.name.length > 64 ||
            !/^[-A-Za-z0-9_+/]+$/.test(z.name),
        )
      )
        return;
      const names = [...new Set(rows.map((z) => z.name))];
      C.zones.splice(0, C.zones.length, ...names);
      zoneLabels = Object.fromEntries(
        rows.map((z) => [
          z.name,
          typeof z.label === "string" ? z.label : z.name,
        ]),
      );
      zonesFetched = true;
    } catch (e) {
      if (e.status === 401)
        throw e; /* Older compatible firmware keeps its original catalogue. */
    }
  }
  const formatTime = (n) =>
    C.formatTimestamp(
      n,
      S.lang,
      S.config?.timezone || "UTC",
      S.config?.clock24 !== false,
    )?.text || t("never");
  function noteSpace(value) {
    const usage = C.noteUsage(value);
    return (
      (S.lang === "pl" ? "Zajęte miejsce: " : "Space used: ") +
      usage.percent +
      "%" +
      (usage.overfull
        ? S.lang === "pl"
          ? " · skróć wiadomość"
          : " · shorten your message"
        : "")
    );
  }
  function updateNoteUsage(value) {
    const usage = C.noteUsage(value),
      label = $("#note-count"),
      meter = $("#note-meter");
    if (label) {
      label.textContent = noteSpace(value);
      label.classList.toggle("error-text", usage.overfull);
    }
    if (meter) {
      meter.value = Math.min(100, usage.percent);
      meter.setAttribute("aria-label", noteSpace(value));
    }
  }
  function credits() {
    const a = (href, label) =>
      `<a href="${href}" target="_blank" rel="noopener noreferrer" referrerpolicy="no-referrer">${label}</a>`;
    return `<div class="credit-row"><span>${S.lang === "pl" ? "Pogoda:" : "Weather:"}</span>${a("https://www.met.no/", "MET Norway")}<span>${S.lang === "pl" ? "Przybliżona lokalizacja:" : "Approximate location:"}</span>${a("https://freeipapi.com/", "FreeIPAPI")}<span>${S.lang === "pl" ? "Wyszukiwanie miejscowości:" : "Place search:"}</span>${a("https://open-meteo.com/", "Open-Meteo")}${a("https://www.geonames.org/", "GeoNames")}</div><p>${S.lang === "pl" ? "Home łączy się bezpośrednio z wybranymi źródłami, a wyszukiwanie miejscowości idzie prosto z telefonu. Dostawcy widzą te zapytania." : "Home contacts your chosen data providers directly, and place search goes straight from your phone. The providers can see those requests."}</p>`;
  }
  function networkHandoff() {
    const relevant =
      S.wifiRequested ||
      (location.hostname === "192.168.4.1" && S.status?.online);
    const url = S.status?.online ? C.localDeviceURL(S.status.address) : null;
    const otherAddress = url && new URL(url).hostname !== location.hostname;
    return `<section id="network-handoff" class="network-handoff" ${!relevant ? "hidden" : ""}><h3>${S.lang === "pl" ? "Dokończ w domowej sieci" : "Continue on your home network"}</h3><p>${S.lang === "pl" ? "Połącz telefon z tym samym domowym Wi-Fi co Home. Otwórz adres urządzenia i sparuj telefon ponownie — telefon zapamiętuje dostęp osobno dla każdego adresu." : "Connect your phone to the same home Wi-Fi as Home. Open its address and pair your phone again — your phone remembers access separately for each address."}</p>${otherAddress ? lanAccessAction() : `<p class="hint">${say("The local address appears after Home connects. Check its screen if this page disconnects.", "Adres pojawi się po połączeniu Home. Jeśli ta strona się rozłączy, sprawdź ekran urządzenia.")}</p>`}<p class="hint">${t("pairNote")}</p></section>`;
  }
  function updateHandoff() {
    const block = $("#network-handoff");
    if (block) block.outerHTML = networkHandoff();
  }
  function nativePreview(kind) {
    let buffer, title;
    if (kind === "confirmed") {
      buffer = S.frame;
      title =
        t("confirmed") +
        (S.frameGeneration !== null ? " · " + S.frameGeneration : "");
    } else if (S.dirty) {
      const cached = S.draftFrames[S.selected];
      buffer =
        cached?.signature === JSON.stringify(S.draft) ? cached.buffer : null;
      title = S.lang === "pl" ? "Podgląd szkicu" : "Draft preview";
    } else {
      buffer = S.frames[S.selected];
      title = t("savedPreview");
    }
    if (!buffer) {
      notice(t("previewMissing"), true);
      return;
    }
    dialog(
      `<h2>${esc(title)}</h2><p>400 × 300 · ${S.lang === "pl" ? "natywny rozmiar" : "native size"}</p><div class="native-scroll" tabindex="0" role="group" aria-label="${S.lang === "pl" ? "Przesuń obraz, aby zobaczyć brzegi" : "Pan the image to see its edges"}"><canvas id="native-frame" width="400" height="300" role="img" aria-label="${esc(title)}"></canvas></div><p class="hint">${S.lang === "pl" ? "Przesuń obraz w bok, aby obejrzeć każdy piksel. To podgląd w telefonie; kolory są przybliżeniem." : "Swipe sideways to inspect every pixel. This is a phone preview; colours are an approximation."}</p>`,
      true,
    );
    paint($("#native-frame"), buffer);
  }
  function notice(message, error = false) {
    clearTimeout(S.noticeTimer);
    const n = $("#notice");
    n.textContent = message;
    n.className = "notice" + (error ? " error" : "");
    n.hidden = false;
    S.noticeTimer = setTimeout(() => (n.hidden = true), error ? 11000 : 6500);
  }
  function locale() {
    document.body.classList.add("app-v2");
    document.documentElement.lang = S.lang;
    document.documentElement.dataset.theme = S.theme;
    $("#language").textContent = S.lang === "en" ? "PL" : "EN";
    $("#language").setAttribute("aria-label", t("changeLanguage"));
    $("#theme").innerHTML = icon(S.theme === "dark" ? "sun" : "moon");
    $("#theme").setAttribute("aria-label", t("theme"));
    $("#reconnect").textContent = t("retry");
    document
      .querySelectorAll("[data-i18n]")
      .forEach((el) => (el.textContent = t(el.dataset.i18n)));
    document.querySelector(".skip").textContent =
      S.lang === "pl" ? "Przejdź do treści" : "Skip to content";
    $("#review .dialog-close").setAttribute("aria-label", t("dismiss"));
    $("#source-credits").innerHTML = credits();
    connection();
  }
  function connection() {
    const e = $("#connection");
    e.className = "connection " + (S.online ? "live" : "offline");
    $("#connection-text").textContent = S.online
      ? t("connected")
      : t("offline");
    $("#reconnect").hidden = S.online;
    if (S.config) updateStatus();
  }
  function expire(message = t("expired")) {
    stopScanPolling();
    clearTimeout(S.wifiTimer);
    S.scan = { state: "idle", networks: [], error: "" };
    S.scanStarted = false;
    S.networkChoice = null;
    S.wifiStep = "list";
    S.wifiRequested = false;
    S.wifiAwaitingStatus = false;
    S.setupDismissed = false;
    S.token = "";
    write("home.token", null);
    S.config = null;
    S.draft = null;
    S.dirty = false;
    S.frame = null;
    S.frames = {};
    S.draftFrames = {};
    S.frameGeneration = null;
    clearTimeout(S.previewTimer);
    S.previewController?.abort();
    $("#review").close();
    renderPair(message);
  }
  async function request(
    path,
    { method = "GET", body, binary = false, publicCall = false } = {},
  ) {
    const controller = new AbortController(),
      timer = setTimeout(() => controller.abort(), 15000),
      requestToken = S.token;
    try {
      const headers = {};
      if (!publicCall && S.token) headers.Authorization = "Bearer " + S.token;
      if (body !== undefined) headers["Content-Type"] = "application/json";
      const r = await fetch(path, {
        method,
        headers,
        body: body === undefined ? undefined : JSON.stringify(body),
        signal: controller.signal,
        cache: "no-store",
        credentials: "omit",
        redirect: "error",
        referrerPolicy: "no-referrer",
      });
      if (!r.ok) {
        const err = new Error("http_" + r.status);
        err.status = r.status;
        try {
          const detail = await r.json();
          if (
            typeof detail?.error === "string" &&
            /^[a-z][a-z0-9_]{0,79}$/.test(detail.error)
          )
            err.code = detail.error;
        } catch {
          /* Keep non-JSON/verbose errors out of the interface. */
        }
        if (r.status === 401 && !publicCall && requestToken === S.token)
          expire();
        throw err;
      }
      S.online = true;
      connection();
      if (binary)
        return {
          buffer: await r.arrayBuffer(),
          generation: r.headers.get("X-Home-Generation"),
        };
      if (r.status === 204) return {};
      const bodyText = await r.text();
      return bodyText ? JSON.parse(bodyText) : {};
    } catch (e) {
      if (!e.status) {
        S.online = false;
        connection();
      }
      throw e;
    } finally {
      clearTimeout(timer);
    }
  }
  function error(e) {
    if (e.status === 401) return;
    if (e.status === 409) {
      S.conflict = true;
      render();
      notice(t("conflict"), true);
    } else
      notice(
        t(e.status === 429 || e.status === 503 ? "busy" : "requestFailed"),
        true,
      );
  }
  function paint(canvas, buf) {
    if (!canvas || !buf) return;
    canvas.width = 400;
    canvas.height = 300;
    canvas
      .getContext("2d")
      .putImageData(new ImageData(C.decodeFrame(buf), 400, 300), 0, 0);
  }
  function paintAll() {
    document
      .querySelectorAll("canvas[data-screen]")
      .forEach((c) => paint(c, S.frames[c.dataset.screen]));
    paint($("#confirmed-frame"), S.frame);
    const summary = $(".edit-preview summary") || $("#preview-heading"),
      canvas = $(".edit-preview canvas");
    if (summary)
      summary.textContent = S.dirty
        ? S.lang === "pl"
          ? "Podgląd Twojego szkicu"
          : "Preview your draft"
        : t("savedPreview");
    if (canvas && summary)
      canvas.setAttribute("aria-label", summary.textContent);
    if (canvas && S.dirty) {
      const cached = S.draftFrames[S.selected];
      canvas.hidden = !cached || cached.signature !== JSON.stringify(S.draft);
      if (!canvas.hidden) paint(canvas, cached.buffer);
    }
  }
  function queueDraftPreview() {
    clearTimeout(S.previewTimer);
    S.previewEpoch++;
    S.previewController?.abort();
    const epoch = S.previewEpoch;
    paintAll();
    const message = $("#draft-preview");
    if (message && S.dirty) {
      message.hidden = false;
      message.textContent =
        S.lang === "pl"
          ? "Przygotowanie podglądu szkicu…"
          : "Preparing your draft preview…";
    }
    S.previewTimer = setTimeout(async () => {
      if (!S.dirty || !S.token || !S.draft) return;
      if (C.validate(S.draft).length) {
        if (message) message.textContent = t("errorFields");
        return;
      }
      const screen = S.selected,
        signature = JSON.stringify(S.draft),
        body = JSON.stringify(C.previewConfig(S.draft)),
        requestToken = S.token,
        controller = new AbortController();
      S.previewController = controller;
      const timeout = setTimeout(() => controller.abort(), 15000);
      const work = (async () => {
        const r = await fetch("/api/preview?screen=" + screen, {
          method: "POST",
          headers: {
            Authorization: "Bearer " + requestToken,
            "Content-Type": "application/json",
          },
          body,
          signal: controller.signal,
          cache: "no-store",
          credentials: "omit",
          redirect: "error",
          referrerPolicy: "no-referrer",
        });
        if (r.status === 401) {
          if (requestToken === S.token) expire();
          const e = new Error("unauthorized");
          e.status = 401;
          throw e;
        }
        if (!r.ok) throw new Error("preview");
        const buffer = await r.arrayBuffer();
        C.decodeFrame(buffer);
        return buffer;
      })();
      S.previewWork = { screen, signature, promise: work };
      try {
        const buffer = await work;
        if (
          epoch !== S.previewEpoch ||
          !S.config ||
          requestToken !== S.token ||
          signature !== JSON.stringify(S.draft)
        )
          return;
        S.draftFrames[screen] = { buffer, signature };
        paintAll();
        const label = $("#draft-preview");
        if (label) {
          label.hidden = false;
          label.textContent =
            S.lang === "pl"
              ? "Podgląd szkicu · jeszcze niezapisany na Home."
              : "Draft preview · not saved on Home yet.";
        }
        const missing = $("#preview-missing");
        if (missing) missing.hidden = true;
      } catch (err) {
        if (err.name !== "AbortError" && epoch === S.previewEpoch) {
          const label = $("#draft-preview");
          if (label) {
            label.hidden = false;
            label.textContent = t("previewMissing");
          }
        }
      } finally {
        clearTimeout(timeout);
        if (S.previewWork?.promise === work) S.previewWork = null;
      }
    }, 550);
  }
  async function previews() {
    // A saved "cycle" previews as Print, exactly like the draft preview (C.previewConfig).
    const body = C.screens.some((s) => S.config?.styles?.[s] === "cycle")
      ? C.previewConfig(S.config)
      : undefined;
    await Promise.all(
      C.screens.map(async (s) => {
        try {
          const r = await request(
            "/api/preview?screen=" + s,
            body ? { method: "POST", body, binary: true } : { binary: true },
          );
          C.decodeFrame(r.buffer);
          S.frames[s] = r.buffer;
        } catch {
          /* Show explicit unavailable state; never use sample data. */
        }
      }),
    );
    document.querySelectorAll("canvas[data-screen]").forEach((c) => {
      c.hidden = !S.frames[c.dataset.screen];
      paint(c, S.frames[c.dataset.screen]);
    });
    const missing = $("#preview-missing");
    if (missing) missing.hidden = !!S.frames[S.selected];
  }
  async function confirmed() {
    if (!S.token) return;
    try {
      const r = await request("/api/frame", { binary: true });
      C.decodeFrame(r.buffer);
      S.frame = r.buffer;
      S.frameGeneration = r.generation;
      const holder = $("#confirmed-holder");
      if (holder)
        holder.innerHTML = `<canvas id="confirmed-frame" width="400" height="300" role="img" aria-label="${esc(t("confirmed"))}"></canvas>`;
      paint($("#confirmed-frame"), S.frame);
      updateStatus();
    } catch (e) {
      if (e.status === 401) return;
      const placeholder = $("#frame-message");
      if (placeholder) placeholder.textContent = t("frameUnknown");
    }
  }
  function renderPair(message = "") {
    if (window.HomeBoot) window.HomeBoot.ready();
    document.body.classList.add("app-v2", "is-onboarding");
    document.body.classList.remove("has-actions");
    app.innerHTML = `<section class="pair"><p class="eyebrow">${t("pairEyebrow")}</p><h1>${t("pairTitle")}</h1><p class="muted">${t("pairIntro")}</p><div class="pair-art" aria-hidden="true"><span></span><span></span></div><p>${t("pairHelp")}</p>${networkHandoff()}${connectionHelp()}<form id="pair-form"><label class="field"><span>${t("code")}</span><input name="code" class="pair-code" inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6}" maxlength="6" minlength="6" required aria-describedby="pair-error" placeholder="· · · · · ·"></label><button type="submit" class="primary">${t("pair")}</button></form><p id="pair-error" class="error-text" role="alert">${esc(message)}</p><p class="hint">${t("pairNote")}</p></section>`;
  }
  const statusName = () =>
    t(
      S.online &&
        ["ready", "preparing", "refreshing", "error"].includes(S.status?.state)
        ? S.status.state
        : "statusUnknown",
    );
  function weatherEditor() {
    const changed = C.placeKey(S.draft) !== C.placeKey(S.config),
      place = changed ? S.draft : S.config,
      approximate = !changed && S.ipPlace?.key === C.placeKey(S.config);
    return `<section class="town-location"><div class="selected-place"><span class="eyebrow">${changed ? say("New location · not saved yet", "Nowa lokalizacja · jeszcze niezapisana") : say("Location saved on Home", "Lokalizacja zapisana na Home")}</span><strong>${place.location_ready === false ? say("Set your location", "Ustaw lokalizację") : esc(place.location)}</strong><small>${place.location_ready === false ? say("One step to your local weather", "Jeden krok do lokalnej pogody") : esc(place.timezone)}</small>${approximate ? `<p class="hint${S.ipPlace.mismatch ? " error-text" : ""}">${S.ipPlace.mismatch ? say("Approximate location may be wrong. Search for your town.", "Lokalizacja przybliżona może być błędna — wyszukaj swoją miejscowość.") : say("Approximate location from Home’s internet connection. If the town is wrong, search for yours.", "Przybliżona lokalizacja z połączenia internetowego Home. Jeśli miejscowość się nie zgadza, wyszukaj swoją.")}</p>` : ""}</div>${townSearch()}<div class="area-option"><button type="button" data-action="use-location" ${S.dirty || S.mutation || S.areaBusy ? "disabled" : ""}>${icon("globe")}${say("Use my location", "Użyj mojej lokalizacji")}</button><p class="hint">${S.dirty ? say("Save or discard your other changes first.", "Najpierw zapisz lub odrzuć pozostałe zmiany.") : say("Home finds an approximate area from its Internet connection, then saves it and shows the weather.", "Home ustali przybliżoną okolicę z połączenia internetowego, zapisze ją i pokaże pogodę.")}</p><p class="hint">${say("FreeIPAPI sees Home’s public IP. Weather coordinates go to MET Norway. It is an approximation and can point to your internet provider’s town.", "FreeIPAPI widzi publiczne IP Home. Współrzędne pogody trafiają do MET Norway. To przybliżenie, które może wskazać miasto dostawcy internetu.")}</p></div></section>`;
  }
  /* Town search (layer 2, D-HOME-CC-14): the phone asks Open-Meteo directly; nothing is stored. */
  function townSearch() {
    const q = S.search,
      open = townListOpen();
    return `<div class="town-search"><label class="field"><span>${say("Search for your town", "Wyszukaj miejscowość")}</span><input id="town-search" type="search" role="combobox" aria-autocomplete="list" aria-controls="town-results" aria-expanded="${open}" aria-describedby="town-status town-privacy" ${open && q.active >= 0 ? `aria-activedescendant="town-option-${q.active}"` : ""} maxlength="100" autocomplete="off" autocapitalize="words" spellcheck="false" enterkeyhint="search" placeholder="${say("e.g. Warsaw", "np. Warszawa")}" value="${esc(q.query)}"></label><p id="town-status" class="hint town-status" role="status">${esc(townStatus())}</p><ul id="town-results" class="town-results" role="listbox" aria-label="${say("Matching places", "Pasujące miejscowości")}" ${open ? "" : "hidden"}>${open ? townOptions() : ""}</ul><p class="town-credit">${say("Search:", "Wyszukiwanie:")} <a href="https://open-meteo.com/" target="_blank" rel="noopener noreferrer" referrerpolicy="no-referrer">Open-Meteo</a> · <a href="https://www.geonames.org/" target="_blank" rel="noopener noreferrer" referrerpolicy="no-referrer">GeoNames</a> · CC BY 4.0</p><p id="town-privacy" class="hint">${say("What you type goes from this phone to Open-Meteo. Home saves only the place you choose.", "To, co wpiszesz, trafia z telefonu do Open-Meteo. Home zapisuje tylko wybraną miejscowość.")}</p></div>`;
  }
  function townListOpen() {
    return (
      S.search.results.length > 0 &&
      ["ready", "loading"].includes(S.search.state)
    );
  }
  function townOptions() {
    return S.search.results
      .map(
        (p, i) =>
          `<li id="town-option-${i}" role="option" aria-selected="${i === S.search.active}" aria-label="${esc(p.label)}" data-action="town-choose" data-index="${i}"><strong>${esc(p.name)}</strong><span>${esc(p.detail)}</span></li>`,
      )
      .join("");
  }
  function townStatus() {
    const q = S.search;
    if (q.state === "loading") return say("Searching…", "Szukam…");
    if (q.state === "ready")
      return say(
        "Choose your town from the list.",
        "Wybierz miejscowość z listy.",
      );
    if (q.state === "empty")
      return say(
        "No matching places. Check the spelling or try a larger town nearby.",
        "Nie znaleziono miejscowości. Sprawdź pisownię albo wpisz większą miejscowość w pobliżu.",
      );
    if (q.state === "offline")
      return say(
        "Search works when your phone has internet. Set your town once Home is on your home Wi-Fi.",
        "Wyszukiwanie działa, gdy telefon ma internet — ustaw miejscowość po połączeniu Home z domową siecią.",
      );
    if (q.state === "error")
      return say(
        "Search is not responding right now. Try again in a moment.",
        "Wyszukiwarka chwilowo nie odpowiada. Spróbuj za chwilę.",
      );
    if (q.state === "chosen") return q.message;
    return [...q.query.trim()].length === 1
      ? say("Type at least 2 letters.", "Wpisz co najmniej 2 litery.")
      : "";
  }
  function updateTownResults() {
    const input = $("#town-search"),
      list = $("#town-results"),
      status = $("#town-status");
    if (!input || !list || !status) return;
    const q = S.search,
      open = townListOpen(),
      opening = open && list.hidden;
    list.innerHTML = open ? townOptions() : "";
    list.hidden = !open;
    input.setAttribute("aria-expanded", String(open));
    if (open && q.active >= 0) {
      input.setAttribute("aria-activedescendant", "town-option-" + q.active);
      $("#town-option-" + q.active)?.scrollIntoView?.({ block: "nearest" });
    } else input.removeAttribute("aria-activedescendant");
    const message = townStatus();
    if (status.textContent !== message) status.textContent = message;
    if (opening) revealTownResults(input, list);
  }
  function revealTownResults(input, list) {
    // On a phone the fixed save bar, the tab bar and the keyboard can cover a freshly opened list.
    // The attribution line under the list should come into view with it.
    const bar = $(".save-bar"),
      credit = list.nextElementSibling || list,
      view = window.visualViewport?.height || window.innerHeight,
      limit =
        bar && !bar.hidden
          ? Math.min(view, bar.getBoundingClientRect().top)
          : view,
      overflow = credit.getBoundingClientRect().bottom + 12 - limit,
      room = input.getBoundingClientRect().top - 20;
    if (overflow > 0 && room > 0)
      window.scrollBy({ top: Math.min(overflow, room), behavior: "instant" });
  }
  function resetSearch() {
    const q = S.search;
    clearTimeout(q.timer);
    q.controller?.abort();
    Object.assign(q, {
      query: "",
      state: "idle",
      results: [],
      active: -1,
      message: "",
      epoch: q.epoch + 1,
      timer: null,
      controller: null,
    });
  }
  function onTownInput(value) {
    const q = S.search,
      url = C.geocodeURL(value, S.lang);
    clearTimeout(q.timer);
    q.controller?.abort();
    q.epoch++;
    q.query = value;
    q.active = -1;
    // Earlier results stay visible while the next search waits; a too-short query clears them.
    if (!url) q.results = [];
    q.state = url ? "loading" : "idle";
    updateTownResults();
    if (url) q.timer = setTimeout(() => searchTowns(value), 350);
  }
  async function searchTowns(query) {
    const q = S.search,
      url = C.geocodeURL(query, S.lang);
    if (!url || query !== q.query) return;
    clearTimeout(q.timer);
    q.controller?.abort();
    const controller = new AbortController(),
      epoch = ++q.epoch;
    let timedOut = false;
    q.controller = controller;
    q.state = "loading";
    updateTownResults();
    const timer = setTimeout(() => {
      timedOut = true;
      controller.abort();
    }, 8000);
    try {
      const r = await fetch(url, {
        signal: controller.signal,
        cache: "no-store",
        credentials: "omit",
        redirect: "error",
        referrerPolicy: "no-referrer",
      });
      if (!r.ok) throw new Error("geocode_http");
      const results = C.geocodeResults(await r.json(), C.zones);
      if (epoch !== q.epoch) return;
      q.results = results;
      q.state = results.length ? "ready" : "empty";
    } catch (e) {
      if (epoch !== q.epoch) return; // A newer search aborted this one and owns the state.
      q.results = [];
      // fetch() rejects with TypeError when the phone has no route to the internet (AP mode).
      q.state = timedOut || e?.name === "TypeError" ? "offline" : "error";
    } finally {
      clearTimeout(timer);
      if (q.controller === controller) q.controller = null;
    }
    q.active = -1;
    updateTownResults();
  }
  function chooseTown(index, fromKeyboard = false) {
    const place = S.search.results[index];
    if (!place || !S.draft) return;
    S.draft = C.searchPlace(S.draft, place);
    resetSearch();
    S.search.state = "chosen";
    S.search.message =
      say("Chosen: ", "Wybrano: ") +
      place.label +
      say(". Now save your settings.", ". Teraz zapisz ustawienia.") +
      (place.timezone
        ? ""
        : say(" The time zone stays as it was.", " Strefa czasu bez zmian."));
    render();
    if (fromKeyboard) $("#town-search")?.focus();
    queueDraftPreview();
  }
  function townKeydown(e) {
    const q = S.search,
      n = q.results.length,
      open = townListOpen();
    if (e.key === "ArrowDown" || e.key === "ArrowUp") {
      if (!open) return;
      e.preventDefault();
      q.active =
        e.key === "ArrowDown"
          ? (q.active + 1) % n
          : q.active <= 0
            ? n - 1
            : q.active - 1;
      updateTownResults();
    } else if (e.key === "Enter") {
      e.preventDefault();
      if (open && q.active >= 0) chooseTown(q.active, true);
      else if (C.geocodeURL(q.query, S.lang)) searchTowns(q.query);
    } else if (e.key === "Escape" && (open || q.state === "loading")) {
      e.preventDefault();
      clearTimeout(q.timer);
      q.controller?.abort();
      q.epoch++;
      Object.assign(q, { results: [], active: -1, state: "idle" });
      updateTownResults();
    }
  }
  /* Compositions only differ when the renderer has data for the screen (docs/qc/D-PODGLAD-H1.md). */
  function styleReady(screen) {
    const c = S.config,
      d = S.draft,
      sources = S.status?.sources;
    if (!c || !d) return false;
    if (screen === "weather")
      return (
        c.location_ready !== false &&
        d.location_ready !== false &&
        d.latitude === c.latitude &&
        d.longitude === c.longitude &&
        sources?.weather?.valid === true
      );
    if (screen === "feed")
      return (
        d.feed_url === c.feed_url &&
        sources?.feed?.valid === true &&
        !!sources.feed.title
      );
    if (screen === "sky")
      return (
        c.location_ready !== false &&
        d.location_ready !== false &&
        d.latitude === c.latitude &&
        d.longitude === c.longitude
      );
    if (screen === "air")
      return (
        c.location_ready !== false &&
        d.location_ready !== false &&
        d.latitude === c.latitude &&
        d.longitude === c.longitude &&
        sources?.air?.valid === true
      );
    return typeof d.note === "string" && d.note.trim() !== "";
  }
  function styleHints(screen) {
    const timing = field(
      "cycle_min",
      "cycleInterval",
      "number",
      'min="5" max="1440" step="1" inputmode="numeric"',
      "cycleIntervalHelp",
    );
    return `<div class="style-note" id="cycle-hint" ${S.draft.styles[screen] === "cycle" ? "" : "hidden"}><p class="hint">${say("Print, Rhythm and Atlas take turns. The preview shows Print. On the device, Down and Up step through them too.", "Plakat, Rytm i Atlas na zmianę. Podgląd pokazuje Plakat. Na urządzeniu przełączają je też przyciski w dół i w górę.")}</p>${timing}</div><p class="hint style-note" id="style-hint" ${styleReady(screen) ? "hidden" : ""}>${say("Compositions differ once Home has data for this screen.", "Kompozycje różnią się, gdy Home ma dane tego ekranu.")}</p>`;
  }
  function updateStyleHints() {
    if (!S.draft || S.tab !== "screens" || !S.editorOpen) return;
    const cycle = $("#cycle-hint"),
      hint = $("#style-hint");
    if (cycle) cycle.hidden = S.draft.styles[S.selected] !== "cycle";
    if (hint) hint.hidden = styleReady(S.selected);
  }
  function phoneTimezone() {
    try {
      return Intl.DateTimeFormat().resolvedOptions().timeZone || "";
    } catch {
      return "";
    }
  }
  function locationPending() {
    const now = Date.now();
    return {
      origin: location.origin,
      started: now,
      expires: now + 300000,
      revision: S.config.revision,
      fingerprint: C.locationFingerprint(S.config),
      timezone: phoneTimezone(),
    };
  }
  function locationProgress(message) {
    dialog(
      `<h2>${say("Setting your weather location", "Ustawianie miejsca pogody")}</h2><p role="status">${esc(message)}</p><button data-modal="cancel">${t("cancel")}</button>`,
    );
  }
  function locationFailure(e, saved = false) {
    if (e.status === 401) return;
    if (e.status === 409 || e.message === "location_conflict") {
      S.conflict = true;
      notice(
        say(
          "Settings changed while locating. Reload the saved settings, then try again.",
          "Ustawienia zmieniły się podczas ustalania miejsca. Wczytaj zapisane ustawienia i spróbuj ponownie.",
        ),
        true,
      );
      render();
      return;
    }
    notice(
      saved
        ? say(
            "Location saved. Weather is not ready yet; your previous picture stays in view. Try Show now after weather updates.",
            "Lokalizacja zapisana. Pogoda nie jest jeszcze gotowa — poprzedni obraz pozostaje. Użyj Pokaż teraz po aktualizacji pogody.",
          )
        : say(
            "Location could not be set. Check Home’s Internet connection and try again in a minute.",
            "Nie udało się ustawić lokalizacji. Sprawdź połączenie Home z Internetem i spróbuj za minutę.",
          ),
      true,
    );
  }
  async function useLocation(replace = false) {
    if (!S.config || S.dirty || S.mutation || S.areaBusy) return;
    // An approximate area never silently replaces a saved place (for example one chosen in search).
    if (S.config.location_ready !== false && !replace) {
      dialog(
        `<h2>${say(`Replace “${esc(S.config.location)}” with an approximate location from your internet address?`, `Zastąpić „${esc(S.config.location)}” przybliżoną lokalizacją z adresu internetowego?`)}</h2><p>${say("The estimate can point to your internet provider’s town instead of yours.", "Przybliżenie może wskazać miasto dostawcy internetu zamiast Twojego.")}</p><div class="inline-actions"><button data-modal="cancel">${say("Keep", "Zostaw")}</button><button class="primary" id="confirm-area">${say("Replace", "Zastąp")}</button></div>`,
      );
      $("#confirm-area").onclick = () => {
        $("#review").close();
        useLocation(true);
      };
      return;
    }
    await applyAutomaticLocation(locationPending());
  }
  async function applyAutomaticLocation(pending) {
    if (S.areaBusy || !S.config) return;
    if (
      !pending ||
      Date.now() > pending.expires ||
      Date.now() < pending.started
    ) {
      locationFailure(new Error("location_expired"));
      return;
    }
    if (
      !C.locationUnchanged(S.config, pending) ||
      !C.locationUnchanged(S.draft, pending)
    ) {
      locationFailure(new Error("location_conflict"));
      return;
    }
    S.areaBusy = true;
    let saved = false;
    locationProgress(
      say(
        "Finding an approximate area from Home’s Internet connection…",
        "Ustalam przybliżoną okolicę z połączenia internetowego Home…",
      ),
    );
    const dialogVersion = S.dialogEpoch,
      current = () =>
        S.dialogEpoch === dialogVersion && $("#review").open && !!S.config;
    try {
      let result;
      await request("/api/location", { method: "POST", body: {} });
      const deadline = performance.now() + 120000;
      while (current()) {
        result = await request("/api/location");
        if (!current()) return;
        if (result.state === "ready") {
          C.areaSuggestion(result);
          break;
        }
        if (result.state === "error" || performance.now() > deadline)
          throw new Error("location_failed");
        await new Promise((resolve) => setTimeout(resolve, 1500));
      }
      if (!current()) return;
      const latest = await request("/api/config");
      if (!current()) return;
      if (Date.now() > pending.expires) throw new Error("location_expired");
      if (!C.locationUnchanged(S.draft, pending))
        throw new Error("location_conflict");
      const next = C.automaticLocation(latest, pending, result);
      if (C.validate(next).length) throw new Error("location_invalid");
      const confirmed = await mutate("/api/config", next, "PUT");
      if (!confirmed || C.validate(confirmed).length)
        throw new Error("location_saved_unknown");
      saved = true;
      S.config = C.clone(confirmed);
      S.draft = C.clone(confirmed);
      S.draftFrames = {};
      S.conflict = false;
      S.dirty = false;
      S.selected = "weather";
      S.tab = "screens";
      S.editorOpen = true;
      // Remembered for this page only: the editor marks the saved place as approximate.
      const mismatch = C.zoneMismatch(result.timezone, pending.timezone);
      S.ipPlace = { key: C.placeKey(confirmed), mismatch };
      render();
      const quality =
        say(
          "Approximate Internet location.",
          "Przybliżona lokalizacja internetowa.",
        ) +
        (mismatch
          ? say(
              " Approximate location may be wrong. Search for your town.",
              " Lokalizacja przybliżona może być błędna — wyszukaj swoją miejscowość.",
            )
          : say(
              " If the town is wrong, search for yours.",
              " Jeśli miejscowość się nie zgadza, wyszukaj swoją.",
            ));
      const zone = pending.timezone;
      const zoneNote = C.zones.includes(zone)
        ? ""
        : say(
            " Your existing time zone was kept.",
            " Zachowano obecną strefę czasu.",
          );
      if (!current()) {
        notice(
          say("Location saved.", "Lokalizacja zapisana.") +
            " " +
            quality +
            zoneNote,
        );
        return;
      }
      $("#review-content").innerHTML =
        `<h2>${say("Location saved", "Lokalizacja zapisana")}</h2><p>${esc(quality + zoneNote)}</p><p role="status">${say("Fetching weather before changing the picture…", "Pobieram pogodę przed zmianą obrazu…")}</p><button data-modal="cancel">${t("cancel")}</button>`;
      const weatherDeadline = performance.now() + 60000;
      while (current()) {
        const status = await request("/api/status");
        if (!current()) return;
        if (status.config_revision !== confirmed.revision)
          throw new Error("location_conflict");
        if (status.sources?.weather?.valid === true) {
          if (!confirmed.enabled[0]) {
            notice(
              say(
                "Location saved. Weather is disabled in your screen settings.",
                "Lokalizacja zapisana. Ekran pogody jest wyłączony w ustawieniach.",
              ),
            );
            return;
          }
          await mutate("/api/show", { screen: "weather" });
          $("#review").close();
          notice(quality + zoneNote + " " + t("showAccepted"));
          await previews();
          return;
        }
        if (performance.now() > weatherDeadline)
          throw new Error("weather_wait_timeout");
        await new Promise((resolve) => setTimeout(resolve, 2000));
      }
    } catch (e) {
      if (current()) {
        $("#review").close();
        locationFailure(e, saved);
      }
    } finally {
      S.areaBusy = false;
      updateDirty();
    }
  }
  function sourceBlock(s) {
    /* Note is typed here and Sky is computed on the device: neither has anything
       to fetch. Air does, so it gets the block and names its provider. */
    if (s === "note" || s === "sky") return "";
    const v = S.status?.sources?.[s] || {};
    const name = [
      "fresh",
      "stale",
      "empty",
      "unknown",
      "fetching",
      "disabled",
      "failed",
    ].includes(v.state)
      ? v.state
      : { ready: "fresh", error: "failed", "unknown-time": "unknown" }[
          v.state
        ] || "empty";
    return `<details class="source-details"><summary>${say("Source & updates", "Źródło i aktualizacje")}</summary><div class="source-state" data-source="${s}"><strong>${t("source")} · ${t(s)}</strong><span>${t(name)}</span><dl>${[
      ["issued", "issued_at"],
      ["fetched", "fetched_at"],
      ["checked", "checked_at"],
      ["expires", "expires_at"],
    ]
      .map(([l, k]) => `<dt>${t(l)}</dt><dd>${esc(formatTime(v[k]))}</dd>`)
      .join(
        "",
      )}</dl><p class="hint">${S.lang === "pl" ? "Godziny według:" : "Times shown in:"} ${esc(C.formatTimestamp(1, S.lang, S.config?.timezone || "UTC", S.config?.clock24 !== false)?.zone || "UTC")}</p>${v.error ? `<p class="error-text">${t("failed")}</p>` : ""}${s === "air" ? `<p class="hint">${t("airSource")}</p>` : ""}<button data-action="refresh" data-source="${s}">${t("refreshSource")}</button></div></details>`;
  }
  function screenView() {
    return S.editorOpen ? editor() : overview();
  }
  function rhythmView() {
    return `<section class="rhythm-page"><div class="page-title"><span class="title-icon">${icon("clock")}</span><div><h1>${t("rhythm")}</h1><p>${say("Give your day a little room.", "Daj swojemu dniu trochę miejsca.")}</p></div></div>${!S.status?.time_valid ? `<p class="inline-feedback error">${icon("info")}${t("timeMissing")}</p>` : ""}<div class="rhythm-layout"><section class="form-section"><h2>${t("mode")}</h2><div class="option-set mode-options">${[
      ["fixed", "screens"],
      ["day", "weather"],
      ["rotate", "refresh"],
    ]
      .map(
        ([m, i]) =>
          `<button data-action="mode" data-mode="${m}" aria-pressed="${S.draft.mode === m}">${icon(i)}<span>${t(m)}</span></button>`,
      )
      .join(
        "",
      )}</div><div class="fields schedule-options">${S.draft.mode === "fixed" ? select("fixed_screen", "fixedScreen", screenOptions(S.draft.fixed_screen)) : S.draft.mode === "rotate" ? field("interval_min", "interval", "number", 'min="5" max="1440" step="1" inputmode="numeric"', "intervalHelp") : `<p class="hint">${t("dayHelp")}</p>${S.draft.day.map((d, i) => `<div class="day-slot"><span class="day-number">${i + 1}</span>${field("day." + i + ".time", "time", "time")}${select("day." + i + ".screen", "screen", screenOptions(d.screen))}</div>`).join("")}`}</div><div class="section-label"><h2>${t("days")}</h2></div><div class="days">${t(
      "dayNames",
    )
      .map(
        (d, i) =>
          `<label title="${t("dayFull")[i]}"><input type="checkbox" data-weekday="${i}" aria-label="${t("dayFull")[i]}" ${S.draft.weekdays & (1 << i) ? "checked" : ""}><span>${d}</span></label>`,
      )
      .join(
        "",
      )}</div></section><div><section class="form-section"><h2>${t("quiet")}</h2>${check("quiet.enabled", "quietEnable")}<div class="row">${field("quiet.start", "start", "time")}${field("quiet.end", "end", "time")}</div><p class="hint">${t("quietHelp")}</p></section><section class="form-section"><h2>${t("pause")}</h2><div class="fields">${field("pause_min", "pauseMinutes", "number", 'min="0" max="240" step="1" inputmode="numeric"', "pauseHelp")}<div class="inline-actions"><button data-action="pause">${icon("pause")}${t("pauseNow")}</button><button data-action="resume">${icon("play")}${t("resume")}</button></div></div></section></div></div></section>`;
  }
  function settingsView() {
    return (
      {
        menu: settingsMenu,
        wifi: () => wifiView(false),
        battery: batterySettings,
        appearance: appearanceSettings,
        preferences: preferencesSettings,
        device: deviceSettings,
      }[S.settingsPage] || settingsMenu
    )();
  }
  function shareView() {
    return `<section class="share-page"><div class="page-title"><span class="title-icon">${icon("share")}</span><div><h1>${say("Pass it on.", "Podaj dalej.")}</h1><p>${t("shareIntro")}</p></div></div><div class="share-layout"><section class="share-hero"><p class="eyebrow">${t("recipeEyebrow")}</p><h2>${t("recipeTitle")}</h2><div class="raster" aria-hidden="true"></div><p>${t("recipeCopy")}</p><button data-action="export-recipe">${icon("share")}${t("exportRecipe")}</button></section><div><section class="form-section"><h2>${t("imageTitle")}</h2><p class="hint">${t("imageCopy")}</p><button data-action="image" class="spaced-action">${icon("screens")}${t("reviewImage")}</button></section><section class="form-section"><h2>${t("importTitle")}</h2><p class="hint">${t("importCopy")}</p><div class="fields"><label class="file-label">${icon("screens")}${t("chooseRecipe")}<input type="file" id="recipe-file" accept="application/json,.json" aria-label="${t("chooseRecipe")}"></label></div></section></div></div></section>`;
  }
  function saveBar() {
    return `<div class="save-bar ${S.dirty ? "dirty" : ""}" ${!S.dirty && !(S.tab === "screens" && S.editorOpen) ? "hidden" : ""}><span class="save-copy" id="save-copy">${t(S.dirty ? "unsaved" : "saved")}</span><button class="edit-actions" data-action="edit-actions" aria-label="${say("More edit actions", "Więcej opcji edycji")}" ${!S.dirty ? "hidden" : ""}>${icon("more")}</button><button data-action="save" class="${S.dirty ? "primary" : ""}" ${!S.dirty ? "disabled" : ""}>${icon("check")}${t("save")}</button>${S.tab === "screens" && S.editorOpen ? `<button data-action="show" class="${!S.dirty ? "primary" : ""}" ${S.dirty || !S.config.enabled[C.screens.indexOf(S.selected)] ? "disabled" : ""}>${icon("play")}${t("show")}</button>` : ""}</div>`;
  }
  function setupBanner() {
    return `<div class="setup-banner" id="setup-banner" ${!S.status?.setup ? "hidden" : ""}><div><strong>${S.lang === "pl" ? "Jeszcze jeden krok." : "One more step."}</strong><p>${S.lang === "pl" ? "Połącz Home z domowym Wi-Fi, aby sam pobierał informacje." : "Connect Home to your Wi-Fi so it can fetch information on its own."}</p></div><button data-action="wifi-settings">${t("wifiConnect")}</button></div>`;
  }
  function render() {
    if (!S.config) {
      renderPair();
      return;
    }
    const onboarding = shouldOnboard();
    if (onboarding) {
      S.tab = "settings";
      S.settingsPage = "wifi";
      S.editorOpen = false;
    }
    document.body.classList.add("app-v2");
    document.body.classList.toggle("is-onboarding", onboarding);
    const view = onboarding
      ? wifiView(true)
      : {
          screens: screenView,
          rhythm: rhythmView,
          settings: settingsView,
          share: shareView,
        }[S.tab]();
    app.innerHTML = `<div class="app-shell">${S.conflict ? `<div class="error-banner" role="alert">${t("conflict")}<br><button data-action="reload">${t("reload")}</button></div>` : ""}<section id="tab-panel" ${onboarding ? `aria-label="${say("Set up Home", "Konfiguracja Home")}"` : `role="tabpanel" aria-labelledby="tab-${S.tab}"`}>${view}</section>${saveBar()}</div>${onboarding ? "" : navigation()}`;
    paintAll();
    updateStatus();
    updateDirty();
    if (window.HomeBoot) window.HomeBoot.ready();
    if (onboarding && !S.scanStarted) setTimeout(() => startScan(false), 0);
  }
  function updateDirty() {
    if (!S.config) return;
    S.dirty = JSON.stringify(S.draft) !== JSON.stringify(S.config);
    const locate = $("[data-action=use-location]");
    if (locate)
      locate.disabled = S.dirty || S.mutation || S.areaBusy || S.conflict;
    const b = $(".save-bar");
    if (!b) return;
    b.hidden = !S.dirty && !(S.tab === "screens" && S.editorOpen);
    document.body.classList.toggle("has-actions", !b.hidden);
    b.classList.toggle("dirty", S.dirty);
    $("#save-copy").textContent = t(
      !S.online ? "offlineDraft" : S.dirty ? "unsaved" : "saved",
    );
    const edits = $("[data-action=edit-actions]");
    if (edits) edits.hidden = !S.dirty;
    const save = $("[data-action=save]");
    save.disabled = !S.dirty || S.mutation || S.conflict || !S.online;
    save.classList.toggle("primary", S.dirty);
    save.innerHTML = icon("check") + t(S.saveInFlight ? "saving" : "save");
    const show = $("[data-action=show]");
    if (show) {
      show.disabled =
        S.dirty ||
        S.mutation ||
        !S.online ||
        !S.config.enabled[C.screens.indexOf(S.selected)];
      show.classList.toggle("primary", !S.dirty);
    }
    document
      .querySelectorAll("[data-action=show-card]")
      .forEach(
        (button) =>
          (button.disabled =
            S.dirty ||
            S.mutation ||
            !S.online ||
            !S.config.enabled[C.screens.indexOf(button.dataset.screen)]),
      );
    const p = $("#draft-preview");
    if (p) p.hidden = !S.dirty;
    updateStyleHints();
  }
  function updateStatus() {
    updateHandoff();
    if (!S.config) return;
    updateHomeCards();
    updateBattery();
    updateSelectedStory();
    if (shouldOnboard() && !$("#wifi-panel")) {
      render();
      return;
    }
    if (S.wifiStep === "connecting") updateWifiContent();
    const ds = $("#display-state");
    if (ds) ds.textContent = statusName();
    const f = $("#frame-label");
    if (f)
      f.textContent =
        t(S.frame ? "confirmed" : "unconfirmed") +
        (S.frameGeneration !== null ? " · " + S.frameGeneration : "");
    const n = $("#refresh-note");
    if (n)
      n.textContent = ["preparing", "refreshing"].includes(S.status?.state)
        ? t("pendingHint") + " " + t("refreshHint")
        : S.status?.pending
          ? t("pendingHint")
          : t("frameHint");
    const g = $("#settings-generation");
    if (g) g.textContent = S.frameGeneration ?? "—";
    const st = $("#settings-state");
    if (st) st.textContent = statusName();
    const sh = $("#source-holder");
    if (sh && S.selected !== "note") {
      const key =
        S.lang +
        "/" +
        S.config.revision +
        "/" +
        JSON.stringify(S.status?.sources?.[S.selected] || {});
      if (sh.dataset.stateKey !== key) {
        const open = sh.querySelector("details")?.open,
          focused = sh.contains(document.activeElement);
        sh.innerHTML = sourceBlock(S.selected);
        sh.dataset.stateKey = key;
        if (open && sh.querySelector("details"))
          sh.querySelector("details").open = true;
        if (focused) sh.querySelector("button")?.focus();
      }
    }
    const setup = $("#setup-banner");
    if (setup) setup.hidden = !S.status?.setup;
    const zoom = $("[data-action=native-preview][data-kind=confirmed]");
    if (zoom) zoom.disabled = !S.frame;
    const mode = $("#mode-summary");
    if (mode) mode.textContent = modeSummary();
    updateDirty();
  }
  async function load() {
    if (!S.token) {
      renderPair();
      return;
    }
    try {
      await loadTimezones();
      const config = await request("/api/config");
      if (C.validate(config).length) throw new Error("config_schema");
      S.config = C.clone(config);
      S.draft = C.clone(config);
      S.draftFrames = {};
      S.dirty = false;
      S.conflict = false;
      resetSearch();
      render();
      await Promise.all([previews(), confirmed()]);
    } catch (e) {
      if (e.status !== 401) {
        if (!S.config)
          app.innerHTML = `<div class="pair"><h1>${t("onScreen")}</h1><p class="error-text">${t("offline")}</p>${connectionHelp()}<button data-action="reload">${t("retry")}</button></div>`;
        if (window.HomeBoot) window.HomeBoot.ready();
        error(e);
      }
    }
  }
  async function poll() {
    if (S.polling || document.hidden) return;
    S.polling = true;
    const pollStarted = performance.now();
    try {
      const old = S.status?.generation,
        oldSources = C.sourcesKey(S.status);
      S.status = await request("/api/status", { publicCall: !S.token });
      if (S.wifiAwaitingStatus && pollStarted >= S.wifiAcceptedAt)
        S.wifiAwaitingStatus = false;
      if (
        S.config &&
        S.status.config_revision !== undefined &&
        S.status.config_revision !== S.config.revision &&
        !S.mutation
      ) {
        if (!S.conflict) {
          S.conflict = true;
          render();
        }
      }
      updateStatus();
      if (
        S.config &&
        (old !== S.status.generation || S.frameGeneration === null)
      )
        await confirmed();
      // New source data (H1): previews rendered before it arrived show the placeholder screen.
      const newSources = C.sourcesKey(S.status);
      if (
        S.config &&
        oldSources !== null &&
        newSources !== null &&
        newSources !== oldSources
      ) {
        S.draftFrames = {};
        await previews();
        if (S.dirty) queueDraftPreview();
      }
    } catch (e) {
      if (e.status !== 401) {
        S.online = false;
        connection();
        updateDirty();
      }
    } finally {
      S.polling = false;
    }
  }
  async function mutate(path, body, method = "POST") {
    if (S.mutation) {
      const e = new Error("busy");
      e.status = 503;
      throw e;
    }
    S.mutation = true;
    updateDirty();
    try {
      return await request(path, { method, body });
    } finally {
      S.mutation = false;
      updateDirty();
    }
  }
  function download(blob, name) {
    const a = document.createElement("a"),
      url = URL.createObjectURL(blob);
    a.href = url;
    a.download = name;
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  function dialog(html, native = false) {
    S.dialogEpoch = (S.dialogEpoch || 0) + 1;
    $("#review").classList.toggle("native-dialog", native);
    $("#review-content").innerHTML = html;
    $("#review").showModal();
  }
  function confirmAction(message, action) {
    dialog(
      `<h2>${t("onScreen")}</h2><p>${esc(message)}</p><div class="inline-actions"><button data-modal="cancel">${t("cancel")}</button><button class="primary" id="confirm-action">${t("reload")}</button></div>`,
    );
    $("#confirm-action").textContent = action.label;
    $("#confirm-action").onclick = async () => {
      $("#review").close();
      await action.run();
    };
  }
  async function save() {
    // The minutes field is hidden unless a screen is In turn: never block a save on it.
    if (
      !C.screens.some((s) => S.draft.styles?.[s] === "cycle") &&
      C.validate(S.draft).includes("cycle_min")
    )
      S.draft.cycle_min = S.config?.cycle_min ?? 30;
    const errors = C.validate(S.draft);
    if (errors.length) {
      notice(
        errors.includes("note")
          ? S.lang === "pl"
            ? "Wiadomość zajmuje za dużo miejsca. Skróć ją do 100% lub mniej."
            : "Your message uses too much space. Shorten it to 100% or less."
          : t("validation") +
              " " +
              errors
                .map(
                  (k) =>
                    ({
                      feed_url: t("feedURL"),
                      large_text: t("largeText"),
                      interval_min: t("interval"),
                      pause_min: t("pauseMinutes"),
                      cycle_min: t("cycleInterval"),
                      ok_action: t("okAction"),
                      air_main: t("airMain"),
                      clock24: t("clock"),
                      weekdays: t("days"),
                    })[k] || t(k),
                )
                .join(", "),
        true,
      );
      for (const key of errors) {
        const f = app.querySelector(`[data-path="${key}"]`);
        if (f) {
          f.setAttribute("aria-invalid", "true");
          f.focus();
          break;
        }
      }
      return;
    }
    const submitted = C.clone(S.draft);
    try {
      S.saveInFlight = true;
      const next = await mutate("/api/config", submitted, "PUT");
      if (!next || !S.config) return;
      if (C.validate(next).length) throw new Error("config_schema");
      const editedWhileSaving =
        JSON.stringify(S.draft) !== JSON.stringify(submitted);
      S.config = C.clone(next);
      if (editedWhileSaving) S.draft.revision = next.revision;
      else S.draft = C.clone(next);
      S.draftFrames = {};
      S.conflict = false;
      S.dirty = editedWhileSaving;
      if (S.search.state === "chosen") resetSearch();
      render();
      notice(t("saveDone"));
      await previews();
      if (editedWhileSaving) queueDraftPreview();
    } catch (e) {
      error(e);
    } finally {
      S.saveInFlight = false;
      updateDirty();
    }
  }
  async function reviewRecipe(file) {
    if (!file) return;
    if (file.size > 16384) {
      notice(t("recipeSize"), true);
      return;
    }
    try {
      const raw = JSON.parse(await file.text()),
        recipe = C.safeRecipe(raw);
      const removed = Object.keys(raw).some(
        (k) => k !== "schema" && !C.recipeKeys.includes(k),
      );
      dialog(
        `<h2>${t("recipeReview")}</h2><p>${t("recipeReviewCopy")}</p>${removed ? `<p>${t("privateRemoved")}</p>` : ""}<dl class="device-facts"><dt>${t("mode")}</dt><dd>${t(recipe.mode)}</dd><dt>${t("texture")}</dt><dd>${recipe.texture} px</dd><dt>${t("intensity")}</dt><dd>${[t("soft"), t("balanced"), t("bold")][recipe.intensity]}</dd><dt>${t("yourScreens")}</dt><dd>${recipe.order.map((s) => t(s)).join(" / ")}</dd></dl><details><summary>${t("importSummary")}</summary><pre>${esc(JSON.stringify(recipe, null, 2))}</pre></details><p class="hint">${t("noPrivate")}</p><div class="inline-actions"><button data-modal="cancel">${t("cancel")}</button><button class="primary" id="confirm-import">${t("importRecipe")}</button></div>`,
      );
      $("#confirm-import").onclick = async () => {
        if (S.dirty) {
          notice(t("savingBlocked"), true);
          return;
        }
        try {
          $("#confirm-import").disabled = true;
          await mutate("/api/recipe", recipe);
          $("#review").close();
          await load();
          notice(t("imported"));
        } catch (e) {
          error(e);
          const b = $("#confirm-import");
          if (b) b.disabled = false;
        }
      };
    } catch {
      notice(t("recipeError"), true);
    }
  }
  function sample(canvas) {
    canvas.width = 400;
    canvas.height = 300;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#e6e5db";
    ctx.fillRect(0, 0, 400, 300);
    for (let y = 0; y < 96; y++)
      for (let x = 0; x < 400; x++) {
        ctx.fillStyle = x % 8 < Math.floor(y / 12) ? "#7b0001" : "#f7ad01";
        ctx.fillRect(x, y, 1, 1);
      }
    ctx.fillStyle = "#1a1a16";
    ctx.font = "bold 12px monospace";
    ctx.fillText(t("sample") + " / emini.ink", 24, 124);
    ctx.font = "bold 40px sans-serif";
    ctx.fillText(t("sampleTitle"), 22, 178);
    ctx.fillText(t("sampleTitle2"), 22, 224);
    ctx.fillRect(24, 244, 352, 1);
    ctx.font = "11px monospace";
    ctx.fillText(t("sampleSub"), 24, 273);
    const pixels = ctx.getImageData(0, 0, 400, 300);
    pixels.data.set(C.quantizeRGBA(pixels.data));
    ctx.putImageData(pixels, 0, 0);
  }
  function imageReview() {
    dialog(
      `<h2>${t("imageReview")}</h2><p>${t("imagePrivacy")}</p><div class="inline-actions"><button id="sample-choice" aria-pressed="true">${t("publicSample")}</button><button id="personal-choice" aria-pressed="false">${t("personalImage")}</button></div><canvas id="share-canvas" width="400" height="300" role="img" aria-label="${t("palette")}"></canvas><p id="image-label">${t("sampleLabel")}</p><button id="download-image" class="primary">${t("downloadImage")}</button>`,
    );
    const canvas = $("#share-canvas");
    sample(canvas);
    let kind = "sample";
    $("#sample-choice").onclick = () => {
      kind = "sample";
      sample(canvas);
      $("#image-label").textContent = t("sampleLabel");
      $("#sample-choice").setAttribute("aria-pressed", "true");
      $("#personal-choice").setAttribute("aria-pressed", "false");
      $("#download-image").disabled = false;
    };
    $("#personal-choice").onclick = async () => {
      const btn = $("#personal-choice");
      btn.disabled = true;
      $("#download-image").disabled = true;
      try {
        const r = await request("/api/share-frame", { binary: true });
        paint(canvas, r.buffer);
        kind = "personal";
        $("#image-label").textContent =
          t("confirmed") + " · " + (r.generation || "—");
        $("#sample-choice").setAttribute("aria-pressed", "false");
        btn.setAttribute("aria-pressed", "true");
        $("#download-image").disabled = false;
      } catch (e) {
        error(e);
        notice(t("refreshUnavailable"), true);
      } finally {
        btn.disabled = false;
      }
    };
    $("#download-image").onclick = () => {
      canvas.toBlob((blob) => {
        if (blob) download(blob, "emini-home-" + kind + ".png");
      }, "image/png");
    };
  }
  app.addEventListener("input", (e) => {
    const f = e.target;
    if (!S.draft || !f.dataset.path) return;
    const keys = f.dataset.path.split(".");
    let target = S.draft;
    for (const k of keys.slice(0, -1)) target = target[k];
    target[keys[keys.length - 1]] =
      f.type === "checkbox"
        ? f.checked
        : f.type === "number" ||
            ["texture", "intensity"].includes(f.dataset.path)
          ? f.value === ""
            ? NaN
            : Number(f.value)
          : f.value;
    f.removeAttribute("aria-invalid");
    if (f.dataset.path === "note") updateNoteUsage(f.value);
    updateDirty();
  });
  app.addEventListener("change", (e) => {
    const f = e.target;
    if (f.dataset.weekday !== undefined) {
      const bit = 1 << Number(f.dataset.weekday);
      S.draft.weekdays = f.checked
        ? S.draft.weekdays | bit
        : S.draft.weekdays & ~bit;
      updateDirty();
    }
    if (f.id === "recipe-file") reviewRecipe(f.files[0]);
  });
  app.addEventListener("click", async (e) => {
    const b = e.target.closest("[data-action]");
    if (!b || b.disabled) return;
    const a = b.dataset.action;
    try {
      if (a === "tab") {
        go(b.dataset.tab);
        $("#tab-" + S.tab)?.focus();
      } else if (a === "select") {
        S.selected = b.dataset.screen;
        S.tab = "screens";
        S.editorOpen = true;
        render();
        window.scrollTo({ top: 0, behavior: "instant" });
      } else if (a === "mode") {
        S.draft.mode = b.dataset.mode;
        render();
      } else if (a === "move") {
        const i = S.draft.order.indexOf(S.selected),
          j = i + Number(b.dataset.direction);
        if (j >= 0 && j < C.screens.length)
          [S.draft.order[i], S.draft.order[j]] = [
            S.draft.order[j],
            S.draft.order[i],
          ];
        render();
      } else if (a === "profile") {
        S.draft = C.profile(S.draft, b.dataset.profile);
        render();
        notice(t("profileApplied"));
      } else if (a === "save") await save();
      else if (a === "reload") {
        if (S.dirty)
          confirmAction(t("reloadConfirm"), { label: t("reload"), run: load });
        else await load();
      } else if (a === "show") {
        await showChosen(S.selected);
      } else if (a === "show-card") {
        await showChosen(b.dataset.screen);
      } else if (a === "refresh") {
        await mutate("/api/refresh", { source: b.dataset.source });
        notice(t("sourceQueued"));
        setTimeout(poll, 1000);
      } else if (a === "pause" || a === "resume") {
        await mutate("/api/pause", { minutes: a === "pause" ? 60 : 0 });
        notice(t(a === "pause" ? "paused" : "resumed"));
      } else if (a === "unpair") {
        confirmAction(t("forgetConfirm"), {
          label: t("forget"),
          run: async () => {
            try {
              await mutate("/api/unpair", {});
              expire("");
              notice(t("forgot"));
            } catch (err) {
              error(err);
            }
          },
        });
      } else if (a === "export-recipe") {
        if (S.dirty) {
          notice(t("savingBlocked"), true);
          return;
        }
        const recipe = C.safeRecipe(await request("/api/recipe"));
        download(
          new Blob([JSON.stringify(recipe, null, 2) + "\n"], {
            type: "application/json",
          }),
          "emini-home-recipe.json",
        );
      } else if (a === "image") imageReview();
    } catch (err) {
      error(err);
    }
  });
  app.addEventListener("keydown", (e) => {
    if (
      e.ctrlKey ||
      e.metaKey ||
      e.altKey ||
      e.target.getAttribute("role") !== "tab" ||
      !["ArrowLeft", "ArrowRight", "Home", "End"].includes(e.key)
    )
      return;
    e.preventDefault();
    const tabs = ["screens", "rhythm", "settings", "share"];
    let i = tabs.indexOf(S.tab);
    i =
      e.key === "Home"
        ? 0
        : e.key === "End"
          ? 3
          : (i + (e.key === "ArrowRight" ? 1 : 3)) % 4;
    go(tabs[i]);
    $("#tab-" + S.tab)?.focus();
  });
  app.addEventListener("submit", async (e) => {
    e.preventDefault();
    const f = e.target,
      button = f.querySelector("[type=submit]");
    if (f.id === "pair-form") {
      const code = f.elements.code.value.trim();
      if (!/^\d{6}$/.test(code)) return;
      button.disabled = true;
      try {
        const result = await request("/api/pair", {
          method: "POST",
          body: { code },
          publicCall: true,
        });
        if (typeof result.token !== "string" || !result.token)
          throw new Error("token");
        S.token = result.token;
        write("home.token", S.token);
        f.reset();
        await load();
        await poll();
      } catch (err) {
        const p = $("#pair-error");
        if (p)
          p.textContent =
            err.status === 409 && err.code === "pairing_limit_reached"
              ? say(
                  "Home already has four saved browser connections. On a phone you no longer use, open Settings → Your device → Disconnect this phone, then try again.",
                  "Home ma już cztery zapisane połączenia przeglądarki. Na nieużywanym telefonie otwórz Ustawienia → Twoje urządzenie → Odłącz ten telefon, a potem spróbuj ponownie.",
                )
              : t(
                  err.status === 429
                    ? "pairLimit"
                    : err.status
                      ? "pairError"
                      : "offline",
                );
      } finally {
        button.disabled = false;
      }
    } else if (f.id === "wifi-form") {
      const ssid = f.elements.ssid.value,
        password = f.elements.password.value;
      if (!ssid || C.bytes(ssid) > 32) {
        notice(t("wifiEmpty"), true);
        return;
      }
      if (C.bytes(password) < 8 || C.bytes(password) > 63) {
        notice(t("validation") + " " + t("password"), true);
        return;
      }
      button.disabled = true;
      try {
        await mutate("/api/wifi", { ssid, password });
        f.reset();
        S.networkChoice = { ...(S.networkChoice || {}), ssid };
        S.phoneOnHomeWiFi = false;
        S.wifiRequested = true;
        S.wifiAwaitingStatus = true;
        S.wifiAcceptedAt = performance.now();
        S.wifiStep = "connecting";
        S.wifiTimedOut = false;
        render();
        beginWifiConfirmation();
      } catch (err) {
        error(err);
      } finally {
        if (f.elements.password) f.elements.password.value = "";
        button.disabled = false;
      }
    }
  });
  $("#review").addEventListener("click", (e) => {
    if (e.target.dataset.modal === "cancel") $("#review").close();
  });
  app.addEventListener("click", (e) => {
    if (
      e.target.closest("[data-action]")?.dataset.action ===
      "phone-network-changed"
    ) {
      S.phoneOnHomeWiFi = true;
      updateHandoff();
      updateWifiContent();
    }
  });
  app.addEventListener("click", (e) => {
    const b = e.target.closest("[data-action=native-preview]");
    if (b && !b.disabled) nativePreview(b.dataset.kind);
  });
  app.addEventListener("click", async (e) => {
    const b = e.target.closest("[data-action]");
    if (!b || b.disabled) return;
    if (b.dataset.action === "appearance-preview") await previewAppearance(b);
    else if (
      b.dataset.action === "texture-choice" ||
      b.dataset.action === "intensity-choice"
    ) {
      S.draft[b.dataset.action === "texture-choice" ? "texture" : "intensity"] =
        Number(b.dataset.value);
      render();
      queueDraftPreview();
    } else if (b.dataset.action === "edit-actions") discardMenu();
  });
  app.addEventListener("change", (e) => {
    if (
      e.target.id === "appearance-screen" &&
      C.screens.includes(e.target.value)
    ) {
      S.selected = e.target.value;
      render();
      queueDraftPreview();
    }
  });
  app.addEventListener("click", async (e) => {
    const b = e.target.closest("[data-action]");
    if (!b || b.disabled) return;
    const action = b.dataset.action;
    if (action === "editor-back") go("screens");
    else if (action === "settings-back") {
      if (S.returnToEditor && S.settingsPage === "appearance") {
        S.tab = "screens";
        S.editorOpen = true;
        S.returnToEditor = false;
        render();
        window.scrollTo({ top: 0, behavior: "instant" });
      } else go("settings");
    } else if (action === "settings-page") {
      S.returnToEditor = false;
      go("settings", b.dataset.page);
    } else if (action === "appearance-settings") {
      go("settings", "appearance");
      S.returnToEditor = true;
    } else if (action === "open-rhythm") go("rhythm");
    else if (action === "wifi-scan") await startScan(true);
    else if (action === "wifi-choose") {
      const network = S.scan.networks[Number(b.dataset.network)];
      if (!network?.supported) return;
      stopScanPolling();
      S.networkChoice = network;
      S.wifiStep = "password";
      render();
    } else if (action === "wifi-manual") {
      stopScanPolling();
      S.networkChoice = null;
      S.wifiStep = "manual";
      render();
    } else if (action === "wifi-list") {
      clearTimeout(S.wifiTimer);
      S.wifiStep = "list";
      S.wifiTimedOut = false;
      render();
      startScan(false);
    } else if (action === "wifi-retry") {
      clearTimeout(S.wifiTimer);
      S.wifiStep = S.networkChoice ? "password" : "manual";
      S.wifiTimedOut = false;
      render();
    } else if (action === "wifi-later" || action === "wifi-finish") {
      S.setupDismissed = true;
      S.wifiRequested = false;
      clearTimeout(S.wifiTimer);
      go("screens");
    } else if (action === "password-toggle") {
      const input = $("#wifi-password");
      if (!input) return;
      const visible = input.type === "password";
      input.type = visible ? "text" : "password";
      b.innerHTML = icon(visible ? "eyeOff" : "eye");
      b.setAttribute("aria-pressed", String(visible));
      b.setAttribute(
        "aria-label",
        visible
          ? say("Hide password", "Ukryj hasło")
          : say("Show password", "Pokaż hasło"),
      );
    } else if (action === "refresh-all") {
      try {
        await mutate("/api/refresh", { source: "all" });
        notice(t("sourceQueued"));
      } catch (err) {
        error(err);
      }
    }
  });
  app.addEventListener("input", (e) => {
    if (e.target.dataset.path) queueDraftPreview();
  });
  app.addEventListener("change", (e) => {
    if (e.target.dataset.weekday !== undefined) queueDraftPreview();
  });
  app.addEventListener("click", (e) => {
    const action = e.target.closest("[data-action]")?.dataset.action;
    if (["select", "mode", "move", "profile"].includes(action))
      queueDraftPreview();
  });
  app.addEventListener("click", (e) => {
    if (e.target.closest("[data-action]")?.dataset.action === "wifi-settings")
      go("settings", "wifi");
  });
  app.addEventListener("click", (e) => {
    if (e.target.closest("[data-action]")?.dataset.action === "use-location")
      useLocation();
  });
  app.addEventListener("input", (e) => {
    if (e.target.id === "town-search") onTownInput(e.target.value);
  });
  app.addEventListener("keydown", (e) => {
    if (e.target.id === "town-search") townKeydown(e);
  });
  app.addEventListener("mousedown", (e) => {
    // Keep focus in the search field while a result is picked with a pointer.
    if (e.target.closest("[data-action=town-choose]")) e.preventDefault();
  });
  app.addEventListener("click", (e) => {
    const option = e.target.closest("[data-action=town-choose]");
    if (option) chooseTown(Number(option.dataset.index));
  });
  $("#language").onclick = () => {
    S.lang = S.lang === "en" ? "pl" : "en";
    write("home.language", S.lang);
    resetSearch(); // Results and messages are in the previous language.
    $("#review").close();
    locale();
    render();
  };
  $("#theme").onclick = () => {
    S.theme = S.theme === "light" ? "dark" : "light";
    write("home.theme", S.theme);
    locale();
  };
  $("#reconnect").onclick = async () => {
    await poll();
    if (S.token && !S.config) await load();
  };
  window.addEventListener("beforeunload", (e) => {
    if (S.dirty) {
      e.preventDefault();
      e.returnValue = "";
    }
  });
  document.addEventListener("visibilitychange", () => {
    if (!document.hidden) poll();
  });
  locale();
  if (S.token) load();
  else renderPair();
  poll();
  setInterval(poll, 6000);
})();
