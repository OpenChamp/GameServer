# Openchamp GameServer Implementation Notes

# 0. Basic Networking
- Instantiate Network instance with open Enet Server for clients to connect to
- Finish client handshakes and verify when lobby is at capacity
- Close

# 1. Adding Basic Objects
- Add map on lobby full, parse NavMesh data from scene.tscn and send level name to clients
- Create Structs for Minons, which should have a Stats and Movement Component available
- Add simple wave spawning mechanic with timeouts to despawn minions after 3s

# 2. Pathfinding and Movement
- Move minions along a predetermined path (Path3D in Godot to provide array of points) and despawn at end
- Flip path for enemy minions and have waves spawn both minions based on Marker3D nodes with group "Minion_Spawner" (check team group to know which belongs to which)

# 3. Basic combat
- Allow minons to damage eachother server-side
- Make minions despawn when health hits 0
- Create some form of state machine for minions to toggle between idle, moving, targeting, attacking, and dead

