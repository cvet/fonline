namespace FOnlineEngine
{
    public class Component
    {
        private Entity _owner;

        [NotImplemented]
        protected virtual void OnInit(bool firstTime) { }
        protected virtual void OnFinish() { }
        protected virtual void OnLoop() { }

        internal void ConstructComponent(Entity owner)
        {
            _owner = owner;
        }
    }
}
