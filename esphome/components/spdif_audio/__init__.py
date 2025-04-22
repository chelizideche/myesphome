import esphome.codegen as cg

CODEOWNERS = ["@johnboiles"]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

spdif_audio_ns = cg.esphome_ns.namespace("spdif_audio")
