# engine v0.2.0

a random game engine thing. Now finally with ECS!
  
![A screenshot](screenshots/2024_03_19.JPG)

# TO DO LIST

Add support for multiple lights.

Support dynamic shadow mapping (probably with cascades)

Support animations.
 - skinned meshes / morph targets

At some point, add game controller support. Make sure it works well with the
InputManager class.

(Was implemented in the past)
For font rendering, put all ASCII characters in one large texture and use
'instancing' (and uniform buffer objects?) to reduce draw calls.

Sort out LOG_X calls and ensure DEBUG, TRACE, INFO, etc are being used appropriately

# DONE

For mesh rendering, give every mesh-renderer a ShaderMaterial which, depending
on the shader, defines how the mesh reacts to light and also stores a reference
to its texture(s). -- Also make a model loader that works with multiple meshes
(by creating many objects).

The engine needs an event/message system, this will be helpful for collision
detection. Also helpful for general gameplay logic.

The entire vulkan backend needs redesigning without so many classes

Place all instances of a particular component in contiguous memory: I.e., a
scene holds many std::vectors, one for each type of component. These vectors are
looped through every frame. This should optimise things by improving the memory
layout of the program, significantly reducing cache misses.

Implemented glTF file loader

Added a PBR shader with albedo, normal, metallic, roughness, and AO textures

Added the BVH AABB tree made in Summer to start a much better Collision system.

The CameraControllerSystem now uses raycasting to enable FPS-style player movement.

Added support for simple shadow mapping.