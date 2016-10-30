/* @flow */

import createFreezed from './create-freezed'
import tap from 'tap';

tap.test('create-freezed', t => {
  const obj = createFreezed({toString() {return 'beep'}}, {a: 'foo'});
  t.equal(obj.a, 'foo', 'has correct assigned value');
  t.equal(obj.toString(), 'beep', 'has correct prototype');
  t.equal(Object.isFrozen(obj), true, 'is frozen');

  t.end();
});
