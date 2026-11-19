/**
 * Identity fallback for Actor FX dissolve coverage.
 *
 * This module is attached only when the optional authored Actor FX module did
 * not compile.  Keeping the same public interface lets core material/shadow
 * shader families remain available while Actor FX degrades to a strict no-op.
 */

bool actorFxDissolveEnabled()
{
    return false;
}

float actorFxDissolveCoverage(vec3 object_position)
{
    return 1.0;
}

bool actorFxDissolveDiscard(vec3 object_position)
{
    return false;
}
