Allows the control of multiple Hamulight drivers with one ESPHome Device


hamulight:
  id: remote_wohnzimmer
  rf_transmit_pin: GPIO27 # Pin für RF-Modul
  rf_address: 0xC535 # Adresse der 1. Fernbedienung

hamulight:
  id: remote_schlafzimmer
  rf_transmit_pin: GPIO27 # Kann derselbe Pin sein, wenn nur ein Modul vorhanden ist
  rf_address: 0xA1B2 # Adresse der 2. Fernbedienung

# ... und so weiter für weitere Fernbedienungen

...

light:
  - platform: hamulight
    name: "Licht Wohnzimmer Decke"
    id: licht_wohnzimmer
    hamulight_id: remote_wohnzimmer # Verweist auf die "Wohnzimmer"-Fernbedienung

  - platform: hamulight
    name: "Licht Schlafzimmer Bett"
    id: licht_schlafzimmer
    hamulight_id: remote_schlafzimmer # Verweist auf die "Schlafzimmer"-Fernbedienung

  # ... und so weiter für weitere Lichter

  
