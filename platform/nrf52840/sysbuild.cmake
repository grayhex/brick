# Refuse to build with a signing key that is public (Nordic sample key) or missing.
# The key must be generated locally by scripts/gen_dev_key.sh and passed by
# scripts/build.sh. Production keys are created and kept outside this repository.
if(SB_CONFIG_BOOTLOADER_MCUBOOT)
  set(_key "${SB_CONFIG_BOOT_SIGNATURE_KEY_FILE}")
  if(NOT _key)
    message(FATAL_ERROR "No MCUboot signing key: use scripts/build.sh "
                        "(it runs scripts/gen_dev_key.sh and passes the key).")
  endif()
  if(_key MATCHES "mcuboot/root-" OR _key MATCHES "root-(ec|ed|rsa)")
    message(FATAL_ERROR "Refusing to sign with the public MCUboot sample key: ${_key}")
  endif()
  if(NOT EXISTS "${_key}")
    message(FATAL_ERROR "MCUboot signing key does not exist: ${_key}")
  endif()
  message(STATUS "Brick: MCUboot signing key (DEV): ${_key}")
endif()
