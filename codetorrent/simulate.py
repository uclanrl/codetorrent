from core import pycore

session = pycore.Session(persistent=True)
node1 = session.addobj(cls=pycore.nodes.CoreNode, name="n1")
node2 = session.addobj(cls=pycore.nodes.CoreNode, name="n2")
hub1 = session.addobj(cls=pycore.nodes.HubNode, name="hub1")
node1.newnetif(hub1, ["10.0.0.1/24"])
node2.newnetif(hub1, ["10.0.0.2/24"])

ct = "/home/josh/installs/codetorrent/codetorrent/codetorrent/ct"

node1.cmd([ct,"0","/home/josh/installs/codetorrent/codetorrent/codetorrent/test.txt","64","64","4","-v","10.0.0.255"],wait=False)
node2.cmd([ct,"1","5","-v"],wait=True)
session.shutdown()


