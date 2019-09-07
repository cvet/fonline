class WikiPage:
    def __init__(self, link):
        self.link = link
        self.members = {}
        
    def addMember(self, refid, member_name):
        self.members[refid] = (member_name, len(filter(lambda (n, _): n == member_name, self.members.values())))
    
    def getMemberAnchor(self, refid):
        (name, number) = self.members[refid]
        return name + "_" + str(number)
        
    
