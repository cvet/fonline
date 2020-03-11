using System.Linq;

namespace FOnlineEngine
{
    public abstract class Entity
    {
        public long Id { get; private set; }
        public bool IsDestroyed { get; private set; }

        internal void Init(long id)
        {
            Id = id;
        }

        internal void Destroy()
        {
            IsDestroyed = true;
        }
    }

    public class Custom : Entity
    {
    }

    public class Item : Entity
    {
    }

    public class Critter : Entity
    {
#if CLIENT
        public bool IsChosen { get { return Id == InternalCalls.GetChosenId(); } }
#endif
    }

    public class Map : Entity
    {
    }

    public class Location : Entity
    {
    }
}
