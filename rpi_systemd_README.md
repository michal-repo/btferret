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