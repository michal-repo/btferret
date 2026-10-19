Compile

`gcc keymouse.c btlib.c -lbluetooth -o keymouse`

Set-up systemd service

```
sudo nano /etc/systemd/system/keymouse.service
```
modify path to btferret

```
[Unit]
Description=BTFerret Keymouse HID Bridge
After=bluetooth.service
Requires=bluetooth.service

[Service]
Type=simple
WorkingDirectory=/home/user/btferret
ExecStart=/usr/bin/sudo /home/user/btferret/keymouse
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

This version have additional support for USB KM/KVM switches like (ATEN US224), periodically checking if device was reconnected.
