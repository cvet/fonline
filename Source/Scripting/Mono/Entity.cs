using System;

BEGIN_FONLINE_NAMESPACE();
    public partial class Entity
    {
        public uint Id { get { return 0; } }

        protected IntPtr _entityPtr;
    }
}
