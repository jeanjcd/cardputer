# Third-party notices

M5-Mesh-Hello is licensed under the GNU General Public License v3.0 or later
(see `LICENSE`). It includes, links against, or was written with reference to
the third-party components below. Versions are the exact ones pinned in
`platformio.ini` (or vendored under `lib/`). Each license was checked against
the license file shipped in the installed package unless noted otherwise.

| Component | Version | Use | License |
|---|---|---|---|
| [rweather/Crypto](https://github.com/rweather/arduinolibs) (vendored subset in `lib/crypto/`) | 0.4.0 | AES, CTR, SHA-256, Curve25519 | MIT (`lib/crypto/LICENSE`) |
| [RadioLib](https://github.com/jgromes/RadioLib) | 7.8.1 | SX1262 driver (device build) | MIT |
| [M5Cardputer](https://github.com/m5stack/M5Cardputer) | 1.1.1 | board support (device build) | MIT (per the `SPDX-License-Identifier: MIT` headers in its sources; neither the package nor the repository has a license file). Bundles Adafruit's TCA8418 keypad driver, marked BSD by Adafruit |
| [M5Unified](https://github.com/m5stack/M5Unified) | 0.3.0 | board support (device build) | MIT |
| [M5GFX](https://github.com/m5stack/M5GFX) | 0.3.0 | display and fonts (device build) | MIT (includes third-party fonts under their own permissive licenses; see the M5GFX repository) |
| [IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) | 5.0.0 | transitive dependency of M5Cardputer (device build) | MIT |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) + ESP-IDF | 3.3.12 (platform espressif32 8.0.0) | framework (device build) | LGPL-2.1 (Arduino core) / Apache-2.0 (ESP-IDF), per upstream repositories |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | 2.6.1 | native unit tests only (not in the firmware) | MIT |
| [Meshtastic firmware](https://github.com/meshtastic/firmware) | reference only | protocol behaviour, region table, AES-CCM block layout | GPL-3.0 |
| [wpa_supplicant / hostapd](https://w1.fi/) AES-CCM (via Meshtastic `src/mesh/aes-ccm.cpp`) | reference only | AES-CCM block layout and processing order in `src/mesh/aes_ccm.cpp` | BSD-3-Clause (notice below) |

No Meshtastic source file is copied into this project; the protocol was
reimplemented from the Meshtastic source and documentation. Because Meshtastic
is GPL-3.0, this project is distributed under a compatible license
(GPL-3.0-or-later).

## wpa_supplicant (AES-CCM)

`src/mesh/aes_ccm.cpp` was rewritten for this project, but its block layout and
processing order follow Meshtastic's `src/mesh/aes-ccm.cpp`, which is derived
from wpa_supplicant. The original notice is reproduced here:

```text
Counter with CBC-MAC (CCM) with AES

Copyright (c) 2010-2012, Jouni Malinen <j@w1.fi>

This software may be distributed under the terms of the BSD license.

This software may be distributed, used, and modified under the terms of
BSD license:

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. Neither the name(s) of the above-listed copyright holder(s) nor the
   names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## rweather/Crypto

```text
Copyright (C) 2015-2018 Southern Storm Software, Pty Ltd.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## RadioLib

```text
MIT License

Copyright (c) 2018 Jan Gromeš

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## M5Unified, M5GFX and M5Cardputer

M5Unified and M5GFX ship the license below. M5Cardputer has no license file;
its sources carry `SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD` and
`SPDX-License-Identifier: MIT`. The Adafruit TCA8418 driver bundled inside
M5Cardputer (`src/utility/Adafruit_TCA8418/`) is marked "BSD license" by
Adafruit Industries.

```text
MIT License

Copyright (c) 2021 M5Stack

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## IRremote

```text
MIT License

(c) Copyright 2009 Ken Shirriff http://www.righto.com
(c) Copyright 2016 Rafi Khan
(c) Copyright 2020-2022 Armin Joachimsmeyer et al.

    http://www.opensource.org/licenses/mit-license.php

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Unity (tests only)

```text
The MIT License (MIT)

Copyright (c) 2007-25 Mike Karlesky, Mark VanderVoord, & Greg Williams

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
