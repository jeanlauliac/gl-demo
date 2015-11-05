import immutable from 'immutable'

function toIterableKeyed<K, V>(
  map: immutable._Map<K, V>
): immutable._Iterable_Keyed {
  return (map: any);
}

module.exports = {toIterableKeyed};
