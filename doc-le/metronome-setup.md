# Bitcoin LE - Metronome Setup

This tutorial is a step-by-step guide to connect Bitcoin LE to a metronome. Current metronome is Bitcoin.

## Install and Configure Bitcoin

Bitcoin LE establishes a connection to Bitcoin's daemon in order to check for new beats.
After installing Bitcoin in your computer, the daemon executable (bitcoind.exe) is present in the installation bin folder.

To start the daemon, open a command prompt and type the following:

*cd "[BitcoinInstallationFolder]/bin"*

*bitcoind -rpcport=[DeamonPortNumber] -datadir=[PathToBitcoinDataDir] -rpcallowip=[YourBitcoinLEClientIP] -server=1 -rest=1 -rpcuser=[DaemonUsername] -rpcpassword=[DaemonPassword]*

**SUGGESTION:** for a default Bitcoin installation, we suggest the following values:
```
DeamonPortNumber: 8332
PathToBitcoinDataDir: C:/Users/[YourUsername]/AppData/Roaming/Bitcoin
YourBitcoinLEClientIP: 127.0.0.1
DaemonUsername: Pick a username that makes sense to you (no spaces or weird characters)
DaemonPassword: Pick a secure password, hard to guess, that makes sense to you
```

**NOTE:** it is not possible to have Bitcoin's GUI and the daemon running at the same time. If your GUI is running, do not forget to close it before running the daemon command.

## Install and Configure Bitcoin LE

After Bitcoin is installed and properly configured, the next step is to connect Bitcoin daemon with Bitcoin LE.
There are 2 ways to achieve this:

**Alternative 1 - Edit bitcoin.conf file Manually**

Create or edit your bitcoin.conf (or equivalent if you don't have a default installation [defautlt: C:/Users/YourUsername/AppData/Roaming/BitcoinLE/bitcoin.conf]) and add the following properties:

```javascript
metronomeAddr=[YourBitcoinDaemonIP]
metronomePort=[DeamonPortNumber]
metronomeUser=[DaemonUsername]
metronomePassword=[DaemonPassword]
```

Test that the connection is successfully established and not blocked by a firewall.
That's all folks! Now you are ready to use Bitcoin LE at full speed!

**Alternative 2 - Connect to Bitcoin through Bitcoin LE GUI**

If the metronome information is not present in the bitcoin.conf file, then when you open Bitcoin LE GUI a prompt will be displayed asking you to insert this information. Just add the information as shown in the picture:

<img src="metronome_setup_gui.png" />

Test that the connection is successfully established and not blocked by a firewall.
That's all folks! Now you are ready to use Bitcoin LE at full speed!
