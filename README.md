# Useful commands

- Find the port for flashing the firmware to esp32:

```bash
ls /dev/tty* | grep 'USB'
```

- Allow user to output stuff:

```bash
sudo adduser $USER dialout
```

- Add read and write to all users to a specific port:

```bash
sudo chmod a+rw /dev/ttyUSB0
```
