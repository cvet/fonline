// Hand-finished port of Engine/Source/Scripting/AngelScript/CoreScripts/Sprite.fos (CLIENT-only).
//
// Lifetime differs deliberately from the AngelScript original. AngelScript `~Sprite()` is a deterministic
// refcount destructor that runs synchronously on the game thread, so it can free the native sprite directly.
// In managed code a finalizer runs asynchronously on the Mono GC finalizer thread, where Game.FreeSprite is
// unsafe: it routes through Native.CallMethod (which marshals strings, allocates managed arrays/GC handles, and
// does no domain attach) and mutates the game-thread-owned native SpriteManager. So the native sprite is freed
// only through deterministic, game-thread Dispose()/Unload(); the finalizer never touches native state and only
// logs a leak (quiet after backend teardown, when its captured alive flag is false and the engine already freed
// all sprites wholesale).
// Downstream callers (ported GUI) must Dispose()/Unload() sprites deterministically rather than relying on GC.
//
// The baker globs every CoreScript into all three target assemblies, so this CLIENT-only type is wrapped in
// `#if CLIENT` (mirroring Sprite.fos): it compiles to nothing in the Server/Mapper assemblies, where the
// client-only Game sprite methods do not exist.
#nullable enable
#if CLIENT

namespace FOnline
{
    public sealed class Sprite : System.IDisposable
    {
        public uint Id = 0;
        public SpriteLayout Layout = SpriteLayout.None;
        public isize Size;
        public int Ticks = 0;
        public bool Hidden = false;
        public hstring LoadedName;
        // The field name `Color` shadows the static class FOnline.Color, so the initializer must qualify it
        // (AngelScript's `Color::Neutral` had explicit scope; the ported `Color.Neutral` would bind to this field).
        public ucolor Color = global::FOnline.Color.Neutral;

        // Captured at construction (on the game thread, under the active backend scope) so the finalizer can
        // ask "is MY engine still alive" from the GC thread, where no backend is active.
        private readonly bool[] _backendAlive = Native.GetBackendAliveFlag();

        public Sprite()
        {
            // Empty
        }

        public Sprite(string name)
        {
            bool ok = Load(name);
            Game.Verify(ok, "Failed to load sprite", name);
        }

        public Sprite(hstring name)
        {
            bool ok = Load(name);
            Game.Verify(ok, "Failed to load sprite", name);
        }

        ~Sprite()
        {
            // Runs on the Mono GC finalizer thread, off the game thread -- must not free the native sprite here.
            // During shutdown the engine frees all sprites, so a still-loaded sprite is expected and silent;
            // otherwise it is a real leak (a Sprite never Dispose()d/Unload()ed). Log only the raw hash to avoid
            // an off-thread hstring-table lookup (LoadedName.Value is a plain managed field).
            if (Id != 0 && _backendAlive[0])
            {
                Native.Log("Managed Sprite leaked (not disposed); hstring hash=" + LoadedName.Value.ToString());
            }
        }

        public void Dispose()
        {
            Unload();
            System.GC.SuppressFinalize(this);
        }

        public bool Load(string name, bool separateLoad = false)
        {
            return Load(name.hstr(), separateLoad);
        }

        public bool Load(hstring name, bool separateLoad = false)
        {
            if (name == LoadedName) {
                return true;
            }

            Unload();
            LoadedName = name;

            if (LoadedName != default(hstring)) {
                Id = separateLoad ? Game.LoadSeparateSprite(LoadedName) : Game.LoadSprite(LoadedName);

                if (Id != 0) {
                    Size = Game.GetSpriteSize(Id);
                }
            }

            return Id != 0;
        }

        public void Unload()
        {
            if (Id != 0) {
                Game.FreeSprite(Id);
                Id = 0;
                LoadedName = default(hstring);
                Size = default(isize);
            }
        }

        // alignX/Y = -1 left / top, 0 center, 1 right / bottom
        public void Draw(ipos pos, isize size = default(isize), int alignX = -1, int alignY = -1, SpriteLayout layout = SpriteLayout.None, ucolor color = default(ucolor))
        {
            if (Id == 0 || Hidden) {
                return;
            }

            if (size == default(isize)) {
                size = Size;
            }

            if (alignX == 0) {
                pos.x -= size.width / 2;
            }
            else if (alignX == 1) {
                pos.x -= size.width;
            }
            if (alignY == 0) {
                pos.y -= size.height / 2;
            }
            else if (alignY == 1) {
                pos.y -= size.height;
            }

            if (color == default(ucolor)) {
                color = Color;
            }

            switch (layout != SpriteLayout.None ? layout : Layout) {
            case SpriteLayout.Tile:
                Game.DrawSpritePattern(Id, pos, size, Size, color);
                break;
            case SpriteLayout.Center:
                Game.DrawSprite(Id, new ipos(pos.x + size.width / 2 - Size.width / 2, pos.y + size.height / 2 - Size.height / 2), color, false);
                break;
            case SpriteLayout.Stretch:
                Game.DrawSprite(Id, pos, size, color, false, false);
                break;
            case SpriteLayout.Zoom:
                Game.DrawSprite(Id, pos, size, color, true, false);
                break;
            default:
                Game.DrawSprite(Id, pos, color, false);
                break;
            }
        }

        public void SetFirstFrame()
        {
            Game.StopSprite(Id);
            Game.SetSpriteTime(Id, 0.0f);
        }

        public void SetLastFrame()
        {
            Game.StopSprite(Id);
            Game.SetSpriteTime(Id, 1.0f);
        }

        public void SetFrame(float normalizedTime)
        {
            Game.StopSprite(Id);
            Game.SetSpriteTime(Id, normalizedTime);
        }

        public void Play(bool loop = true, bool reverse = false)
        {
            if (Id == 0) {
                return;
            }

            Game.PlaySprite(Id, default(hstring), loop, reverse);
        }

        public void Play(string animName, bool loop = true, bool reverse = false)
        {
            if (Id == 0) {
                return;
            }

            Game.PlaySprite(Id, animName.hstr(), loop, reverse);
        }

        public void Play(hstring animName, bool loop = true, bool reverse = false)
        {
            if (Id == 0) {
                return;
            }

            Game.PlaySprite(Id, animName, loop, reverse);
        }
    }
}

#endif
