namespace FOnline.Core
{
    public class Tween
    {
        public void Init(int beginX, int beginY, int endX, int endY, uint pixelsPerSecond)
        {
            x1 = beginX;
            y1 = beginY;
            x2 = endX;
            y2 = endY;
            x3 = beginX;
            y3 = beginY;
            end = false;
            pps = pixelsPerSecond;
        }

        public void Move(bool toEnd)
        {
            if (end == toEnd)
                return;
        }

        public void ForceMove(bool toEnd)
        {
            end = toEnd;
            x3 = (end ? x2 : x1);
            y3 = (end ? y2 : y1);
        }

        public int get_CurX()
        {
            return 0;
        }

        public int get_CurY()
        {
            return 0;
        }

        public int get_Dist()
        {
            return 0;
        }

        public int get_Percent()
        {
            return 0;
        }

        private bool end;
        private int x1;
        private int y1;
        private int x2;
        private int y2;
        private int x3;
        private int y3;
        private uint pps;
    }
}
