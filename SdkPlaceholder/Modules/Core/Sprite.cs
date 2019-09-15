using System;

namespace Core
{
    public static class Test
    {
        public static int Foo()
        {
#if SERVER
            Console.WriteLine("SERVER");
#elif CLIENT
            Console.WriteLine("CLIENT");
#elif MAPPER
            Console.WriteLine("MAPPER");
#else
            Console.WriteLine("unknown");
#endif
            return 42;
        }
    }

    public class Sprite
    {
        public Sprite()
        {
            // Empty
        }

        public Sprite(string name)
        {
            bool ok = Load(name);
        }

        public Sprite(uint nameHash)
        {
            bool ok = Load(nameHash);
        }

        public bool Load(string name)
        {
            return Id != 0;
        }

        public bool Load(uint nameHash)
        {
            if (nameHash == LoadedNameHash)
                return true;

            LoadedNameHash = nameHash;
            RefreshData();
            return Id != 0;
        }

        public void Unload()
        {
            Id = 0;
            RefreshData();
        }

        // cornerX/Y = -1 left / top, 0 center, 1 right / bottom
        public void Draw(int x, int y, int w = 0, int h = 0, int cornerX = -1, int cornerY = -1, int layout = -1, uint color = 0)
        {
            if (cornerX == 0)
                x -= w / 2;
            else if (cornerX == 1)
                x -= w;
            if (cornerY == 0)
                y -= h / 2;
            else if (cornerY == 1)
                y -= h;
        }

        public void SetFrame(int index)
        {
            Frame = index;
        }

        public void SetFirstFrame()
        {
            SetFrame(0);
        }

        public void SetLastFrame()
        {
        }

        public void PlayAnimation(bool loop = true, bool reverse = false)
        {
            if (Id == 0)
                return;
        }

        private void RefreshData()
        {
        }

        private uint Id;
        private int Frame = -1;
        private uint LoadedNameHash;
    }
}
