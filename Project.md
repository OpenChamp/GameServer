# Openchamp GameServer Implementation Notes

# 0. Basic Networking
[x] Instantiate Network instance with open Enet Server for clients to connect to 
[x] Finish client handshakes and verify when lobby is at capacity
[x] Close connection

# 1. Adding Basic Objects
[x] Add map on lobby full, parse NavMesh data from scene.tscn and send level name to clients
[x] Create Structs for Minons, which should have a Stats and Movement Component available
[ ] PARTIAL: Add simple wave spawning mechanic with timeouts to despawn minions after 3s

# 2. Pathfinding and Movement
[ ] PARTIAL: Move minions along a predetermined path (Path3D in Godot to provide array of points) and despawn at end
[ ] Flip path for enemy minions and have waves spawn both minions based on Marker3D nodes with group "Minion_Spawner" (check team group to know which belongs to which)

# 3. Basic combat
[ ] PARTIAL: Allow minons to damage eachother server-side
[x] Make minions despawn when health hits 0
[ ] Create some form of state machine for minions to toggle between idle, moving, targeting, attacking, and dead

