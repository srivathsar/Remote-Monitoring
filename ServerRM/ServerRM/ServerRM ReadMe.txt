ServerRM.exe

Application used to monitor client machines on which the ClientRM.exe has been installed. Provides an easy to use GUI for the same.

Menu Options:

Server Config > Client DB > Add : Add a new client machine by entering the IP address and nickname. Duplicate entries are not allowed, i.e., no two clients can have the same IP address.
Server Config > Client DB> Remove: Remove a previously added client machine from the database.
Server Config > Client DB> View Clients: View the previously added client machines and optionally edit the nicknames.

Server Config > Plugin DB > Add: Add a new plugin by specifying a unique PluginID, path of the DLL files and names of the plug-in DLL and interface DLL. Duplicacy not allowed. No two plugins can have the same ID.
Server Config > Plugin DB > Remove: Remove a plugin from the database.
Server Config > Plugin DB > View Plugins: View previously added plugins.

Monitor > Scan IP Addresses: Scan a range of IP addresses in search of machines that have ClientRM.exe installed and running.
Monitor > Connect: Connect to a prreviously added client machine.
Monitor > Launch Plugin: Once connected, launch any of the available plugin monitors.
Monitor > Disconnect: Disconnect from the connected client machine.

Help > About: Display the About Box.