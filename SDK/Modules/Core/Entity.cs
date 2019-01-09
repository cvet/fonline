using System.Linq;

namespace FOnlineEngine
{
    public abstract class Entity
    {
        protected readonly long _id;
        public long Id { get { return _id; } }
        protected readonly Entity _owner;
        public Entity Owner { get { return _owner; } }
        protected readonly Component[] _components;

        internal Entity(long id, Entity owner, byte[] data)
        {
            _id = id;
            _owner = owner;
            //_components = components;
        }

        public T GetComponent<T>() where T : Component
        {
            return _components.FirstOrDefault(c => c is T) as T;
        }
    }

    public class Item : Entity
    {
        public Critter CritterOwner { get { return _owner as Critter; } }
        public Item ItemOwner { get { return _owner as Item; } }

        internal Item(long id, Entity owner, byte[] data) :
            base(id, owner, data)
        {
        }
    }

    public class Critter : Entity
    {
        internal Critter(long id, byte[] data) :
            base(id, null, data)
        {
#if CLIENT
            _isChosen = id == InternalCalls.GetChosenId();
#endif
        }

#if CLIENT
        private readonly bool _isChosen;
        public bool IsChosen { get { return _isChosen; } }
#endif
    }

    public class Map : Entity
    {
        public Location LocationOwner { get { return (Location)_owner; } }

        internal Map(long id, Location location, byte[] data) :
            base(id, location, data)
        {
        }
    }

    public class Location : Entity
    {
        internal Location(long id, byte[] data) :
            base(id, null, data)
        {
        }
    }
}
