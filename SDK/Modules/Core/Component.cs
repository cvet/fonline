namespace FOnlineEngine
{
    public class Component
    {
        private Entity _owner;

        internal void ConstructComponent(Entity owner)
        {
            _owner = owner;
        }
    }
}
