// goto-handler.cpp - must be rejected by --compile6 (S132): a goto out of a catch handler would leave the handled
// exception and the handler's objects alive; a goto that stays inside the handler is accepted
struct B { int v; B(int x) : v(x) {} B(const B& o) : v(o.v + 10) {} ~B() {} };

int byRef() { int r = 0; try { throw B(1); } catch (B& b) { r = b.v; goto out; } out: return r; }
int byValue() { int r = 0; try { throw B(2); } catch (B b) { r = b.v; goto out; } out: return r; }
int inside() { int r = 0; try { throw B(3); } catch (B b) { if (b.v) goto in; r = 1; in: r += b.v; } return r; }

int main() { return byRef() + byValue() + inside(); }
